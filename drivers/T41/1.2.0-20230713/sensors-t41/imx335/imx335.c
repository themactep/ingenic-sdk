/*
 * imx335.c
 *
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          2592*1944       25        mipi_2lane            linear
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

#define IMX335_CHIP_ID_H	(0x38)
#define IMX335_CHIP_ID_L	(0x0a)
#define IMX335_REG_END		0xffff
#define IMX335_REG_DELAY	0xfffe
#define IMX335_SUPPORT_SCLK	(74250000)
#define SENSOR_OUTPUT_MIN_FPS 5
#define AGAIN_MAX_DB 0x64
#define DGAIN_MAX_DB 0x64
#define LOG2_GAIN_SHIFT 16
#define SENSOR_VERSION	"H20220920a"

static int shvflip = 1;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

static int reset_gpio = GPIO_PC(28);
static int pwdn_gpio = -1;

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

struct again_lut imx335_again_lut[] = {
	{0x04, 0},
	{0x05, 38336},
	{0x06, 55574},
	{0x07, 70149},
	{0x08, 82774},
	{0x09, 93910},
	{0x0a, 103872},
	{0x0b, 112883},
	{0x0c, 121110},
	{0x0d, 128678},
	{0x0e, 135685},
	{0x0f, 142208},
	{0x10, 148310},
	{0x11, 154042},
	{0x12, 159446},
	{0x13, 164558},
	{0x14, 169408},
	{0x15, 174021},
	{0x16, 178419},
	{0x17, 182622},
	{0x18, 186646},
	{0x19, 190505},
	{0x1a, 194214},
	{0x1b, 197782},
	{0x1c, 201221},
	{0x1d, 204538},
	{0x1e, 207744},
	{0x1f, 210844},
	{0x20, 213846},
	{0x21, 216755},
	{0x22, 219578},
	{0x23, 222318},
	{0x24, 224982},
	{0x25, 227572},
	{0x26, 230094},
	{0x27, 232550},
	{0x28, 234944},
	{0x29, 237278},
	{0x2a, 239557},
	{0x2b, 241781},
	{0x2c, 243955},
	{0x2d, 246080},
	{0x2e, 248158},
	{0x2f, 250191},
	{0x30, 252182},
	{0x31, 254131},
	{0x32, 256041},
	{0x33, 257914},
	{0x34, 259750},
	{0x35, 261551},
	{0x36, 263318},
	{0x37, 265053},
	{0x38, 266757},
	{0x39, 268430},
	{0x3a, 270074},
	{0x3b, 271691},
	{0x3c, 273280},
	{0x3d, 274843},
	{0x3e, 276380},
	{0x3f, 277893},
	{0x40, 279382},
	{0x41, 280848},
	{0x42, 282291},
	{0x43, 283713},
	{0x44, 285114},
	{0x45, 286494},
	{0x46, 287854},
	{0x47, 289196},
	{0x48, 290518},
	{0x49, 291822},
	{0x4a, 293108},
	{0x4b, 294378},
	{0x4c, 295630},
	{0x4d, 296866},
	{0x4e, 298086},
	{0x4f, 299290},
	{0x50, 300480},
	{0x51, 301654},
	{0x52, 302814},
	{0x53, 303960},
	{0x54, 305093},
	{0x55, 306212},
	{0x56, 307317},
	{0x57, 308410},
	{0x58, 309491},
	{0x59, 310559},
	{0x5a, 311616},
	{0x5b, 312661},
	{0x5c, 313694},
	{0x5d, 314716},
	{0x5e, 315727},
	{0x5f, 316728},
	{0x60, 317718},
	{0x61, 318698},
	{0x62, 319667},
	{0x63, 320627},
	{0x64, 321577},
	{0x65, 322518},
	{0x66, 323450},
	{0x67, 324372},
	{0x68, 325286},
	{0x69, 326191},
	{0x6a, 327087},
	{0x6b, 327975},
	{0x6c, 328854},
	{0x6d, 329725},
	{0x6e, 330589},
	{0x6f, 331445},
	{0x70, 332293},
	{0x71, 333133},
	{0x72, 333966},
	{0x73, 334792},
	{0x74, 335610},
	{0x75, 336422},
	{0x76, 337227},
	{0x77, 338025},
	{0x78, 338816},
	{0x79, 339600},
	{0x7a, 340379},
	{0x7b, 341150},
	{0x7c, 341916},
	{0x7d, 342675},
	{0x7e, 343429},
	{0x7f, 344176},
	{0x80, 344918},
	{0x81, 345654},
	{0x82, 346384},
	{0x83, 347108},
	{0x84, 347827},
	{0x85, 348541},
	{0x86, 349249},
	{0x87, 349952},
	{0x88, 350650},
	{0x89, 351342},
	{0x8a, 352030},
	{0x8b, 352713},
	{0x8c, 353390},
	{0x8d, 354063},
	{0x8e, 354732},
	{0x8f, 355395},
	{0x90, 356054},
	{0x91, 356708},
	{0x92, 357358},
	{0x93, 358003},
	{0x94, 358644},
	{0x95, 359281},
	{0x96, 359914},
	{0x97, 360542},
	{0x98, 361166},
	{0x99, 361786},
	{0x9a, 362402},
	{0x9b, 363014},
	{0x9c, 363622},
	{0x9d, 364226},
	{0x9e, 364826},
	{0x9f, 365423},
	{0xa0, 366016},
	{0xa1, 366605},
	{0xa2, 367190},
	{0xa3, 367772},
	{0xa4, 368350},
	{0xa5, 368925},
	{0xa6, 369496},
	{0xa7, 370064},
	{0xa8, 370629},
	{0xa9, 371190},
	{0xaa, 371748},
	{0xab, 372302},
	{0xac, 372853},
	{0xad, 373402},
	{0xae, 373946},
	{0xaf, 374488},
	{0xb0, 375027},
	{0xb1, 375563},
	{0xb2, 376095},
	{0xb3, 376625},
	{0xb4, 377152},
	{0xb5, 377676},
	{0xb6, 378197},
	{0xb7, 378715},
	{0xb8, 379230},
	{0xb9, 379742},
	{0xba, 380252},
	{0xbb, 380759},
	{0xbc, 381263},
	{0xbd, 381765},
	{0xbe, 382264},
	{0xbf, 382760},
	{0xc0, 383254},
	{0xc1, 383745},
	{0xc2, 384234},
	{0xc3, 384720},
	{0xc4, 385203},
	{0xc5, 385685},
	{0xc6, 386163},
	{0xc7, 386640},
	{0xc8, 387113},
	{0xc9, 387585},
	{0xca, 388054},
	{0xcb, 388521},
	{0xcc, 388986},
	{0xcd, 389448},
	{0xce, 389908},
	{0xcf, 390366},
	{0xd0, 390822},
	{0xd1, 391275},
	{0xd2, 391727},
	{0xd3, 392176},
	{0xd4, 392623},
	{0xd5, 393068},
	{0xd6, 393511},
	{0xd7, 393951},
	{0xd8, 394390},
	{0xd9, 394827},
	{0xda, 395261},
	{0xdb, 395694},
	{0xdc, 396125},
	{0xdd, 396554},
	{0xde, 396981},
	{0xdf, 397406},
	{0xe0, 397829},
	{0xe1, 398250},
	{0xe2, 398669},
	{0xe3, 399086},
	{0xe4, 399502},
	{0xe5, 399916},
	{0xe6, 400328},
	{0xe7, 400738},
	{0xe8, 401146},
	{0xe9, 401553},
	{0xea, 401958},
	{0xeb, 402361},
	{0xec, 402763},
	{0xed, 403162},
	{0xee, 403561},
	{0xef, 403957},
	{0xf0, 404352},
};
struct tx_isp_sensor_attribute imx335_attr;
unsigned int imx335_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = imx335_again_lut;
	while(lut->gain <= imx335_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == imx335_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int imx335_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}



struct tx_isp_sensor_attribute imx335_attr={
	.name = "imx335",
	.chip_id = 0x380a,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x1a,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_SONY_MODE,
		.clk = 1118,
		.lans = 2,
		.settle_time_apative_en = 0,
		.image_twidth = 2616,
		.image_theight = 1964,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.mipi_crop_start0x = 12,
		.mipi_sc.mipi_crop_start0y = 33,
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

	.max_again = 404352,
	.max_again_short = 404352,
	.max_dgain = 0,
	.min_integration_time = 4,
	.min_integration_time_native = 1,
	.max_integration_time_native = 5400 -10,
	.min_integration_time_short = 1,
	.max_integration_time_short = 10,
	.integration_time_limit = 5400 -10,
	.total_width = 550,
	.total_height = 5400,
	.max_integration_time =5400 -10,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = imx335_alloc_again,
	.sensor_ctrl.alloc_dgain = imx335_alloc_dgain,
};


static struct regval_list imx335_init_regs_2592_1944_25fps_mipi[] = {
	{0x3000, 0x01},
	{0x3001, 0x00},
	{0x3002, 0x01},
	{0x3004, 0x00},
	{0x300c, 0x5b},
	{0x300d, 0x40},
	{0x3018, 0x00},
	{0x302c, 0x30},
	{0x302d, 0x00},
	{0x302e, 0x38},
	{0x302f, 0x0a},
	{0x3030, 0x18},//
	{0x3031, 0x15},//HMAX
	{0x3032, 0x00},
	{0x3034, 0x26},//
	{0x3035, 0x02},//VMAX
	{0x3050, 0x00},
	{0x315a, 0x02},
	{0x316a, 0x7e},
	{0x319d, 0x00},
	{0x31a1, 0x00},
	{0x3288, 0x21},
	{0x328a, 0x02},
	{0x3414, 0x05},
	{0x3416, 0x18},
	{0x341c, 0xff},
	{0x341d, 0x01},
	{0x3648, 0x01},
	{0x364a, 0x04},
	{0x364c, 0x04},
	{0x3678, 0x01},
	{0x367c, 0x31},
	{0x367e, 0x31},
	{0x3706, 0x10},
	{0x3708, 0x03},
	{0x3714, 0x02},
	{0x3715, 0x02},
	{0x3716, 0x01},
	{0x3717, 0x03},
	{0x371c, 0x3d},
	{0x371d, 0x3f},
	{0x372c, 0x00},
	{0x372d, 0x00},
	{0x372e, 0x46},
	{0x372f, 0x00},
	{0x3730, 0x89},
	{0x3731, 0x00},
	{0x3732, 0x08},
	{0x3733, 0x01},
	{0x3734, 0xfe},
	{0x3735, 0x05},
	{0x3740, 0x02},
	{0x375d, 0x00},
	{0x375e, 0x00},
	{0x375f, 0x11},
	{0x3760, 0x01},
	{0x3768, 0x1b},
	{0x3769, 0x1b},
	{0x376a, 0x1b},
	{0x376b, 0x1b},
	{0x376c, 0x1a},
	{0x376d, 0x17},
	{0x376e, 0x0f},
	{0x3776, 0x00},
	{0x3777, 0x00},
	{0x3778, 0x46},
	{0x3779, 0x00},
	{0x377a, 0x89},
	{0x377b, 0x00},
	{0x377c, 0x08},
	{0x377d, 0x01},
	{0x377e, 0x23},
	{0x377f, 0x02},
	{0x3780, 0xd9},
	{0x3781, 0x03},
	{0x3782, 0xf5},
	{0x3783, 0x06},
	{0x3784, 0xa5},
	{0x3788, 0x0f},
	{0x378a, 0xd9},
	{0x378b, 0x03},
	{0x378c, 0xeb},
	{0x378d, 0x05},
	{0x378e, 0x87},
	{0x378f, 0x06},
	{0x3790, 0xf5},
	{0x3792, 0x43},
	{0x3794, 0x7a},
	{0x3796, 0xa1},
	{0x3a01, 0x01},
	{0x3002, 0x00},
	{0x3000, 0x00},
	{IMX335_REG_END, 0x00},/* END MARKER */
};


static struct tx_isp_sensor_win_setting imx335_win_sizes[] = {
	{
		.width		= 2592,
		.height		= 1944,
		.fps		= 25 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SRGGB10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= imx335_init_regs_2592_1944_25fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &imx335_win_sizes[0];

static struct regval_list imx335_stream_on_mipi[] = {
	{IMX335_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list imx335_stream_off_mipi[] = {
	{IMX335_REG_END, 0x00},	/* END MARKER */
};

int imx335_read(struct tx_isp_subdev *sd, uint16_t reg,
		unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg >> 8) & 0xff, reg & 0xff};
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

int imx335_write(struct tx_isp_subdev *sd, uint16_t reg,
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

#if 0
static int imx335_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != IMX335_REG_END) {
		if (vals->reg_num == IMX335_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = imx335_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int imx335_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;

	while (vals->reg_num != IMX335_REG_END) {
		if (vals->reg_num == IMX335_REG_DELAY) {
			msleep(vals->value);
		} else {
			ret = imx335_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int imx335_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

static int imx335_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = imx335_read(sd, 0x302e, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != IMX335_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = imx335_read(sd, 0x302f, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != IMX335_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int imx335_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned short shr0 = 0;
	unsigned short vmax = 0;

	vmax = imx335_attr.total_height;
	shr0 = vmax - value;
	ret = imx335_write(sd, 0x3058, (unsigned char)(shr0 & 0xff));
	ret += imx335_write(sd, 0x3059, (unsigned char)((shr0 >> 8) & 0xff));
	ret += imx335_write(sd, 0x305a, (unsigned char)((shr0 >> 16) & 0x0f));
	if (0 != ret) {
		ISP_ERROR("err: imx335_write err\n");
		return ret;
	}

	return 0;
}

static int imx335_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = imx335_write(sd, 0x30e8, (unsigned char)(value & 0xff));
	ret += imx335_write(sd, 0x30e9, (unsigned char)((value >> 8) & 0x07));
	if (ret < 0)
		return ret;

	return 0;
}

static int imx335_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int imx335_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
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
	sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;

	return 0;
}

static int imx335_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if(!init->enable)
		return ISP_SUCCESS;

	sensor_set_attr(sd, wsize);
	sensor->video.state = TX_ISP_MODULE_INIT;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int imx335_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (init->enable) {
		if(sensor->video.state == TX_ISP_MODULE_INIT){
			ret = imx335_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
		if(sensor->video.state == TX_ISP_MODULE_RUNNING){

			ret = imx335_write_array(sd, imx335_stream_on_mipi);
			ISP_WARNING("imx335 stream on\n");
		}
	}
	else {
		ret = imx335_write_array(sd, imx335_stream_off_mipi);
		ISP_WARNING("imx335 stream off\n");
	}

	return ret;
}

static int imx335_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned int max_fps;
	unsigned char tmp;
	unsigned char val = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	switch(sensor->info.default_boot){
	case 0:
		sclk = IMX335_SUPPORT_SCLK;
		max_fps = TX_SENSOR_MAX_FPS_25;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps<< 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%x) no in range\n", fps);
		return -1;
	}

	ret += imx335_read(sd, 0x3035, &val);
	hts = val << 8;
	ret += imx335_read(sd, 0x3034, &val);
	hts = (hts | val);
	if (0 != ret) {
		ISP_ERROR("err: imx335 read err\n");
		return -1;
	}
	hts = (hts << 8) + tmp;//////
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += imx335_write(sd, 0x3001, 0x01);
	ret = imx335_write(sd, 0x3032, (unsigned char)((vts & 0xf0000) >> 16));
	ret += imx335_write(sd, 0x3031, (unsigned char)((vts & 0xff00) >> 8));
	ret += imx335_write(sd, 0x3030, (unsigned char)(vts & 0xff));
	ret += imx335_write(sd, 0x3001, 0x00);
	if (0 != ret) {
		ISP_ERROR("err: imx335_write err\n");
		return ret;
	}
	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 9;
	sensor->video.attr->integration_time_limit = vts - 9;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 9;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int imx335_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if(wsize){
		sensor_set_attr(sd, wsize);
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

#if 1
static int imx335_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val_h;
	uint8_t val_v;
	uint8_t reg_3081 = 0x02;
	uint8_t reg_3083 = 0x02;
	uint16_t reg_st_adr = 0x00c4;

	/*
	 * 2'b01:mirror,2'b10:filp
	 * 0x3081 0x3082 must be changed as blow in all-pixel scan mode
	 */
	ret = imx335_read(sd, 0x304e, &val_h);
	ret += imx335_read(sd, 0x304f, &val_v);
	switch(enable) {
	case 0://normal
		val_h &= 0xfc;
		val_v &= 0xfc;
		reg_3081 = 0x02;
		reg_3083 = 0x02;
		reg_st_adr = 0x00c4;
		break;
	case 1://sensor mirror
		val_h |= 0x01;
		val_v &= 0xfc;
		reg_3081 = 0x02;
		reg_3083 = 0x02;
		reg_st_adr = 0x00c4;
		break;
	case 2://sensor flip
		val_h &= 0xfc;
		val_v |= 0x01;
		reg_3081 = 0xfe;
		reg_3083 = 0xfe;
		reg_st_adr = 0x1000;
		break;
	case 3://sensor mirror&flip
		val_h |= 0x01;
		val_v |= 0x01;
		reg_3081 = 0xfe;
		reg_3083 = 0xfe;
		reg_st_adr = 0x1000;
		break;
	}
	ret = imx335_write(sd, 0x304e, val_h);
	ret += imx335_write(sd, 0x304f, val_v);
	ret += imx335_write(sd, 0x3081, reg_3081);
	ret += imx335_write(sd, 0x3083, reg_3083);
	ret += imx335_write(sd, 0x3075, (reg_st_adr >> 8) & 0xff);
	ret += imx335_write(sd, 0x3074, reg_st_adr & 0xff);

	return ret;
}
#endif

static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	struct clk *sclka;
	unsigned long rate;
	int ret;

	switch(info->default_boot){
	case 0:
		wsize = &imx335_win_sizes[0];
		imx335_attr.mipi.clk = 800;
		imx335_attr.total_width = 550;
		imx335_attr.total_height = 5400;
		imx335_attr.max_integration_time = 5400 - 10;
		imx335_attr.integration_time_limit = 5400 - 10;
		imx335_attr.max_integration_time_native = 5400 - 10;
		imx335_attr.again = 0;
        imx335_attr.integration_time = 9;
		break;
	default:
		ISP_ERROR("Have no this Setting Source!!!\n");
	}

	switch(info->video_interface){
	case TISP_SENSOR_VI_MIPI_CSI0:
		imx335_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		imx335_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_DVP:
		imx335_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		break;
	default:
		ISP_ERROR("Have no this Interface Source!!!\n");
	}

	switch(info->mclk){
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
	switch(info->default_boot){
	case 0:
                if (((rate / 1000) % 37125) != 0) {
                        ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
                        sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
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
                break;
	}

	ISP_WARNING("\n====>[default_boot=%d] [resolution=%dx%d] [video_interface=%d] [MCLK=%d] \n", info->default_boot, wsize->width, wsize->height, info->video_interface, info->mclk);
	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;

	sensor_set_attr(sd, wsize);
	sensor->priv = wsize;
        sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;
	return 0;

}

static int imx335_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"imx335_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(5);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(5);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"imx335_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(5);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(5);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = imx335_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an imx335 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("imx335 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "imx335", sizeof("imx335"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int imx335_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;

	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
	//	if(arg)
	//		ret = imx335_set_expo(sd, sensor_val->value);
	//	break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
		if(arg)
			ret = imx335_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
		if(arg)
			ret = imx335_set_analog_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = imx335_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = imx335_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = imx335_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = imx335_write_array(sd, imx335_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = imx335_write_array(sd, imx335_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = imx335_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = imx335_set_vflip(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static int imx335_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = imx335_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int imx335_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	imx335_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops imx335_core_ops = {
	.g_chip_ident = imx335_g_chip_ident,
	.reset = imx335_reset,
	.init = imx335_init,
	.g_register = imx335_g_register,
	.s_register = imx335_s_register,
};

static struct tx_isp_subdev_video_ops imx335_video_ops = {
	.s_stream = imx335_s_stream,
};

static struct tx_isp_subdev_sensor_ops	imx335_sensor_ops = {
	.ioctl	= imx335_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops imx335_ops = {
	.core = &imx335_core_ops,
	.video = &imx335_video_ops,
	.sensor = &imx335_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "imx335",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int imx335_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
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

	sd = &sensor->sd;
	video = &sensor->video;
	sensor->dev = &client->dev;
	imx335_attr.expo_fs = 1;
	sensor->video.shvflip = 1;
	sensor->video.attr = &imx335_attr;
	tx_isp_subdev_init(&sensor_platform_device, sd, &imx335_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->imx335\n");

	return 0;
}

static int imx335_remove(struct i2c_client *client)
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

static const struct i2c_device_id imx335_id[] = {
	{ "imx335", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, imx335_id);

static struct i2c_driver imx335_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "imx335",
	},
	.probe		= imx335_probe,
	.remove		= imx335_remove,
	.id_table	= imx335_id,
};

static __init int init_imx335(void)
{
	return private_i2c_add_driver(&imx335_driver);
}

static __exit void exit_imx335(void)
{
	private_i2c_del_driver(&imx335_driver);
}

module_init(init_imx335);
module_exit(exit_imx335);

MODULE_DESCRIPTION("A low-level driver for imx335 sensors");
MODULE_LICENSE("GPL");
