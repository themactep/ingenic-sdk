/*
 * sc2337p.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * resolution      fps       interface          mode	DVDD
 * 1920*1080       25        mipi_2lane        linear	interior
 * 1920*1080       30        mipi_1lane        linear	interior
 *
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

#define SC2337P_CHIP_ID_H	(0x9b)
#define SC2337P_CHIP_ID_L	(0x3a)
#define SC2337P_REG_END		0xffff
#define SC2337P_REG_DELAY	0xfffe
#define SC2337P_SUPPORT_25FPS_SCLK (79200000) /* 0x898 * 0X5a0 * 25 */
#define SC2337P_SUPPORT_30FPS_SCLK (74250000) /* 2200*1125*30 */
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20240926a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int shvflip = 1;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

static int sensor_max_fps = TX_SENSOR_MAX_FPS_30;
module_param(sensor_max_fps, int, S_IRUGO);
MODULE_PARM_DESC(sensor_max_fps, "Sensor Max Fps set interface");


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

struct again_lut sc2337p_again_lut[] = {
//cnt_gain = 192 cnt_reg = 192
	{0x80, 0},
	{0x84, 2886},
	{0x88, 5776},
	{0x8c, 8494},
	{0x90, 11136},
	{0x94, 13706},
	{0x98, 16287},
	{0x9c, 18723},
	{0xa0, 21097},
	{0xa4, 23413},
	{0xa8, 25746},
	{0xac, 27952},
	{0xb0, 30108},
	{0xb4, 32216},
	{0xb8, 34344},
	{0xbc, 36361},
	{0xc0, 38335},
	{0xc4, 40269},
	{0xc8, 42225},
	{0xcc, 44082},
	{0xd0, 45903},
	{0xd4, 47689},
	{0xd8, 49499},
	{0xdc, 51220},
	{0xe0, 52910},
	{0xe4, 54570},
	{0xe8, 56253},
	{0xec, 57856},
	{0xf0, 59433},
	{0xf4, 60983},
	{0xf8, 62557},
	{0xfc, 64058},
	{0x880, 65535},
	{0x884, 68421},
	{0x888, 71311},
	{0x88c, 74029},
	{0x890, 76671},
	{0x894, 79241},
	{0x898, 81822},
	{0x89c, 84258},
	{0x8a0, 86632},
	{0x8a4, 88948},
	{0x8a8, 91281},
	{0x8ac, 93487},
	{0x8b0, 95643},
	{0x8b4, 97751},
	{0x8b8, 99879},
	{0x8bc, 101896},
	{0x8c0, 103870},
	{0x8c4, 105804},
	{0x8c8, 107760},
	{0x8cc, 109617},
	{0x8d0, 111438},
	{0x8d4, 113224},
	{0x8d8, 115034},
	{0x8dc, 116755},
	{0x8e0, 118445},
	{0x8e4, 120105},
	{0x8e8, 121788},
	{0x8ec, 123391},
	{0x8f0, 124968},
	{0x8f4, 126518},
	{0x8f8, 128092},
	{0x8fc, 129593},
	{0x980, 131070},
	{0x984, 133956},
	{0x988, 136846},
	{0x98c, 139564},
	{0x990, 142206},
	{0x994, 144776},
	{0x998, 147357},
	{0x99c, 149793},
	{0x9a0, 152167},
	{0x9a4, 154483},
	{0x9a8, 156816},
	{0x9ac, 159022},
	{0x9b0, 161178},
	{0x9b4, 163286},
	{0x9b8, 165414},
	{0x9bc, 167431},
	{0x9c0, 169405},
	{0x9c4, 171339},
	{0x9c8, 173295},
	{0x9cc, 175152},
	{0x9d0, 176973},
	{0x9d4, 178759},
	{0x9d8, 180569},
	{0x9dc, 182290},
	{0x9e0, 183980},
	{0x9e4, 185640},
	{0x9e8, 187323},
	{0x9ec, 188926},
	{0x9f0, 190503},
	{0x9f4, 192053},
	{0x9f8, 193627},
	{0x9fc, 195128},
	{0xb80, 196605},
	{0xb84, 199491},
	{0xb88, 202381},
	{0xb8c, 205099},
	{0xb90, 207741},
	{0xb94, 210311},
	{0xb98, 212892},
	{0xb9c, 215328},
	{0xba0, 217702},
	{0xba4, 220018},
	{0xba8, 222351},
	{0xbac, 224557},
	{0xbb0, 226713},
	{0xbb4, 228821},
	{0xbb8, 230949},
	{0xbbc, 232966},
	{0xbc0, 234940},
	{0xbc4, 236874},
	{0xbc8, 238830},
	{0xbcc, 240687},
	{0xbd0, 242508},
	{0xbd4, 244294},
	{0xbd8, 246104},
	{0xbdc, 247825},
	{0xbe0, 249515},
	{0xbe4, 251175},
	{0xbe8, 252858},
	{0xbec, 254461},
	{0xbf0, 256038},
	{0xbf4, 257588},
	{0xbf8, 259162},
	{0xbfc, 260663},
	{0xf80, 262140},
	{0xf84, 265026},
	{0xf88, 267916},
	{0xf8c, 270634},
	{0xf90, 273276},
	{0xf94, 275846},
	{0xf98, 278427},
	{0xf9c, 280863},
	{0xfa0, 283237},
	{0xfa4, 285553},
	{0xfa8, 287886},
	{0xfac, 290092},
	{0xfb0, 292248},
	{0xfb4, 294356},
	{0xfb8, 296484},
	{0xfbc, 298501},
	{0xfc0, 300475},
	{0xfc4, 302409},
	{0xfc8, 304365},
	{0xfcc, 306222},
	{0xfd0, 308043},
	{0xfd4, 309829},
	{0xfd8, 311639},
	{0xfdc, 313360},
	{0xfe0, 315050},
	{0xfe4, 316710},
	{0xfe8, 318393},
	{0xfec, 319996},
	{0xff0, 321573},
	{0xff4, 323123},
	{0xff8, 324697},
	{0xffc, 326198},
	{0x1f80, 327675},
	{0x1f84, 330561},
	{0x1f88, 333451},
	{0x1f8c, 336169},
	{0x1f90, 338811},
	{0x1f94, 341381},
	{0x1f98, 343962},
	{0x1f9c, 346398},
	{0x1fa0, 348772},
	{0x1fa4, 351088},
	{0x1fa8, 353421},
	{0x1fac, 355627},
	{0x1fb0, 357783},
	{0x1fb4, 359891},
	{0x1fb8, 362019},
	{0x1fbc, 364036},
	{0x1fc0, 366010},
	{0x1fc4, 367944},
	{0x1fc8, 369900},
	{0x1fcc, 371757},
	{0x1fd0, 373578},
	{0x1fd4, 375364},
	{0x1fd8, 377174},
	{0x1fdc, 378895},
	{0x1fe0, 380585},
	{0x1fe4, 382245},
	{0x1fe8, 383928},
	{0x1fec, 385531},
	{0x1ff0, 387108},
	{0x1ff4, 388658},
	{0x1ff8, 390232},
	{0x1ffc, 391733},
};

struct tx_isp_sensor_attribute sc2337p_attr;

unsigned int sc2337p_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc2337p_again_lut;
	while(lut->gain <= sc2337p_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == sc2337p_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sc2337p_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute sc2337p_attr={
	.name = "sc2337p",
	.chip_id = 0x9b3a,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 391733,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = 0x5a0 - 6,
	.integration_time_limit = 0x5a0 - 6,
	.total_width = 0x898,
	.total_height = 0X5a0,
	.max_integration_time = 0x5a0 - 6,
	.one_line_expr_in_us = 27,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sc2337p_alloc_again,
	.sensor_ctrl.alloc_dgain = sc2337p_alloc_dgain,
};

struct tx_isp_mipi_bus sc2337p_mipi_25fps = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 396,
		.lans = 2,
		.settle_time_apative_en = 0,
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

struct tx_isp_mipi_bus sc2337p_mipi_1lane = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 150,
		.lans = 1,
		.settle_time_apative_en = 0,
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

static struct regval_list sc2337p_init_regs_1920_1080_30fps_mipi_1lane[] = {
	{0x0103,0x01},
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x3018,0x12},
	{0x3019,0x0e},
	{0x301f,0x17},
	{0x3106,0x05},
	{0x3248,0x04},
	{0x3249,0x0b},
	{0x3253,0x08},
	{0x3301,0x09},
	{0x3302,0xff},
	{0x3303,0x10},
	{0x3306,0x80},
	{0x3307,0x02},
	{0x3309,0xc8},
	{0x330a,0x01},
	{0x330b,0x30},
	{0x330c,0x16},
	{0x330d,0xff},
	{0x3318,0x02},
	{0x331f,0xb9},
	{0x3321,0x0a},
	{0x3327,0x0e},
	{0x332b,0x12},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x335e,0x06},
	{0x335f,0x0a},
	{0x3364,0x1f},
	{0x337c,0x02},
	{0x337d,0x0e},
	{0x3390,0x09},
	{0x3391,0x0f},
	{0x3392,0x1f},
	{0x3393,0x20},
	{0x3394,0x20},
	{0x3395,0xe0},
	{0x33a2,0x04},
	{0x33b1,0x80},
	{0x33b2,0x68},
	{0x33b3,0x42},
	{0x33f9,0x90},
	{0x33fb,0xd0},
	{0x33fc,0x0f},
	{0x33fd,0x1f},
	{0x349f,0x03},
	{0x34a6,0x0f},
	{0x34a7,0x1f},
	{0x34a8,0x42},
	{0x34a9,0x18},
	{0x34aa,0x01},
	{0x34ab,0x43},
	{0x34ac,0x01},
	{0x34ad,0x80},
	{0x3630,0xf4},
	{0x3632,0x44},
	{0x3633,0x22},
	{0x3639,0xf4},
	{0x363c,0x47},
	{0x3670,0x09},
	{0x3674,0xf4},
	{0x3675,0xfb},
	{0x3676,0xed},
	{0x367c,0x09},
	{0x367d,0x0f},
	{0x3690,0x22},
	{0x3691,0x22},
	{0x3692,0x22},
	{0x3698,0x89},
	{0x3699,0x96},
	{0x369a,0xd0},
	{0x369b,0xd0},
	{0x369c,0x09},
	{0x369d,0x0f},
	{0x36a2,0x09},
	{0x36a3,0x0f},
	{0x36a4,0x1f},
	{0x36d0,0x01},
	{0x36ec,0x0c},
	{0x3722,0xc1},
	{0x3724,0x41},
	{0x3725,0xc1},
	{0x3728,0x20},
	{0x3900,0x0d},
	{0x3905,0x98},
	{0x3919,0x04},
	{0x391b,0x81},
	{0x391c,0x10},
	{0x3933,0x81},
	{0x3934,0xd0},
	{0x3940,0x75},
	{0x3941,0x00},
	{0x3942,0x01},
	{0x3943,0xd1},
	{0x3952,0x02},
	{0x3953,0x0f},
	{0x3e01,0x45},
	{0x3e02,0xf0},
	{0x3e08,0x1f},
	{0x3e1b,0x14},
	{0x4509,0x38},
	{0x5799,0x06},
	{0x5ae0,0xfe},
	{0x5ae1,0x40},
	{0x5ae2,0x30},
	{0x5ae3,0x28},
	{0x5ae4,0x20},
	{0x5ae5,0x30},
	{0x5ae6,0x28},
	{0x5ae7,0x20},
	{0x5ae8,0x3c},
	{0x5ae9,0x30},
	{0x5aea,0x28},
	{0x5aeb,0x3c},
	{0x5aec,0x30},
	{0x5aed,0x28},
	{0x5aee,0xfe},
	{0x5aef,0x40},
	{0x5af4,0x30},
	{0x5af5,0x28},
	{0x5af6,0x20},
	{0x5af7,0x30},
	{0x5af8,0x28},
	{0x5af9,0x20},
	{0x5afa,0x3c},
	{0x5afb,0x30},
	{0x5afc,0x28},
	{0x5afd,0x3c},
	{0x5afe,0x30},
	{0x5aff,0x28},
	{0x36e9,0x20},
	{0x37f9,0x27},
	{0x0100,0x01},
	{SC2337P_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc2337p_init_regs_1920_1080_20fps_mipi[] = {
	{0x0103,0x01},
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x301f,0x09},
	{0x3106,0x05},
	{0x320e,0x07},
	{0x320f,0x08},
	{0x3248,0x04},
	{0x3249,0x0b},
	{0x3253,0x08},
	{0x3301,0x09},
	{0x3302,0xff},
	{0x3303,0x10},
	{0x3306,0x80},
	{0x3307,0x02},
	{0x3309,0xc8},
	{0x330a,0x01},
	{0x330b,0x30},
	{0x330c,0x16},
	{0x330d,0xff},
	{0x3318,0x02},
	{0x331f,0xb9},
	{0x3321,0x0a},
	{0x3327,0x0e},
	{0x332b,0x12},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x335e,0x06},
	{0x335f,0x0a},
	{0x3364,0x1f},
	{0x337c,0x02},
	{0x337d,0x0e},
	{0x3390,0x09},
	{0x3391,0x0f},
	{0x3392,0x1f},
	{0x3393,0x20},
	{0x3394,0x20},
	{0x3395,0xe0},
	{0x33a2,0x04},
	{0x33b1,0x80},
	{0x33b2,0x68},
	{0x33b3,0x42},
	{0x33f9,0x90},
	{0x33fb,0xd0},
	{0x33fc,0x0f},
	{0x33fd,0x1f},
	{0x349f,0x03},
	{0x34a6,0x0f},
	{0x34a7,0x1f},
	{0x34a8,0x42},
	{0x34a9,0x18},
	{0x34aa,0x01},
	{0x34ab,0x43},
	{0x34ac,0x01},
	{0x34ad,0x80},
	{0x3630,0xf4},
	{0x3632,0x44},
	{0x3633,0x22},
	{0x3639,0xf4},
	{0x363c,0x47},
	{0x3670,0x09},
	{0x3674,0xf4},
	{0x3675,0xfb},
	{0x3676,0xed},
	{0x367c,0x09},
	{0x367d,0x0f},
	{0x3690,0x22},
	{0x3691,0x22},
	{0x3692,0x22},
	{0x3698,0x89},
	{0x3699,0x96},
	{0x369a,0xd0},
	{0x369b,0xd0},
	{0x369c,0x09},
	{0x369d,0x0f},
	{0x36a2,0x09},
	{0x36a3,0x0f},
	{0x36a4,0x1f},
	{0x36d0,0x01},
	{0x36ea,0x0b},
	{0x36eb,0x0c},
	{0x36ec,0x1c},
	{0x36ed,0x18},
	{0x3722,0xc1},
	{0x3724,0x41},
	{0x3725,0xc1},
	{0x3728,0x20},
	{0x37fa,0xcb},
	{0x37fb,0x32},
	{0x37fc,0x11},
	{0x37fd,0x07},
	{0x3900,0x0d},
	{0x3905,0x98},
	{0x3919,0x04},
	{0x391b,0x81},
	{0x391c,0x10},
	{0x3933,0x81},
	{0x3934,0xd0},
	{0x3940,0x75},
	{0x3941,0x00},
	{0x3942,0x01},
	{0x3943,0xd1},
	{0x3952,0x02},
	{0x3953,0x0f},
	{0x3e01,0x59},
	{0x3e02,0xa0},
	{0x3e08,0x1f},
	{0x3e1b,0x14},
	{0x4509,0x38},
	{0x4819,0x06},
	{0x481b,0x03},
	{0x481d,0x0b},
	{0x481f,0x02},
	{0x4821,0x08},
	{0x4823,0x03},
	{0x4825,0x02},
	{0x4827,0x03},
	{0x4829,0x04},
	{0x5799,0x06},
	{0x5ae0,0xfe},
	{0x5ae1,0x40},
	{0x5ae2,0x30},
	{0x5ae3,0x28},
	{0x5ae4,0x20},
	{0x5ae5,0x30},
	{0x5ae6,0x28},
	{0x5ae7,0x20},
	{0x5ae8,0x3c},
	{0x5ae9,0x30},
	{0x5aea,0x28},
	{0x5aeb,0x3c},
	{0x5aec,0x30},
	{0x5aed,0x28},
	{0x5aee,0xfe},
	{0x5aef,0x40},
	{0x5af4,0x30},
	{0x5af5,0x28},
	{0x5af6,0x20},
	{0x5af7,0x30},
	{0x5af8,0x28},
	{0x5af9,0x20},
	{0x5afa,0x3c},
	{0x5afb,0x30},
	{0x5afc,0x28},
	{0x5afd,0x3c},
	{0x5afe,0x30},
	{0x5aff,0x28},
	{0x36e9,0x53},
	{0x37f9,0x33},
	{0x0100,0x01},
    {SC2337P_REG_END, 0x00},	/* END MARKER */
};

static struct tx_isp_sensor_win_setting sc2337p_win_sizes[] = {
    {
		.width		= 1920,
		.height		= 1080,
		.fps		= 20 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc2337p_init_regs_1920_1080_20fps_mipi,
	},
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc2337p_init_regs_1920_1080_30fps_mipi_1lane,
	},

};
struct tx_isp_sensor_win_setting *wsize = &sc2337p_win_sizes[1];

static struct regval_list sc2337p_stream_on_mipi[] = {
	{0x0100, 0x01},
	{SC2337P_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc2337p_stream_off_mipi[] = {
	{0x0100, 0x00},
	{SC2337P_REG_END, 0x00},	/* END MARKER */
};

int sc2337p_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
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

int sc2337p_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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
        /* printk("[%s %d] 0x%04x = 0x%02x\n", __func__, __LINE__, reg, value); */

	return ret;
}

#if 0
static int sc2337p_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SC2337P_REG_END) {
		if (vals->reg_num == SC2337P_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc2337p_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int sc2337p_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SC2337P_REG_END) {
		if (vals->reg_num == SC2337P_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc2337p_write(sd, vals->reg_num, vals->value);
			printk("{0x%04x,0x%04x}\n",vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc2337p_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sc2337p_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret = 0;
	unsigned char v;

	ret += sc2337p_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC2337P_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret += sc2337p_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC2337P_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc2337p_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	//integration time
	ret = sc2337p_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0xf));
	ret += sc2337p_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc2337p_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));

	//sensor analog gain
	ret += sc2337p_write(sd, 0x3e09, (unsigned char)(((again >> 8) & 0xff)));
	//sensor dig fine gain
	ret += sc2337p_write(sd, 0x3e07, (unsigned char)(again & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

#if 0
static int sc2337p_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	value *= 2;
	ret += sc2337p_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0xf));
	ret += sc2337p_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc2337p_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));

	if (ret < 0)
		return ret;

	return 0;
}

static int sc2337p_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret += sc2337p_write(sd, 0x3e09, (unsigned char)(value & 0xff));
	ret += sc2337p_write(sd, 0x3e08, (unsigned char)(((value >> 8) & 0xff)));
	if (ret < 0)
		return ret;
	gain_val = value;

	return 0;
}
#endif

static int sc2337p_set_logic(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc2337p_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc2337p_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc2337p_init(struct tx_isp_subdev *sd, int enable)
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

	ret = sc2337p_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sc2337p_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc2337p_write_array(sd, sc2337p_stream_on_mipi);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("sc2337p stream on\n");

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc2337p_write_array(sd, sc2337p_stream_off_mipi);
		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("sc2337p stream off\n");
	}

	return ret;
}

static int sc2337p_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp = 0;
	unsigned int max_fps = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;

	sclk = SC2337P_SUPPORT_25FPS_SCLK;

	switch(sensor_max_fps){
	case TX_SENSOR_MAX_FPS_25:
		sclk = SC2337P_SUPPORT_25FPS_SCLK;
		max_fps = TX_SENSOR_MAX_FPS_25;
		break;
	case TX_SENSOR_MAX_FPS_30:
		sclk = SC2337P_SUPPORT_30FPS_SCLK;
		max_fps = TX_SENSOR_MAX_FPS_30;
		break;
	default:
		ISP_ERROR("do not support max framerate %d in mipi mode\n",sensor_max_fps);
	}

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}

	ret = sc2337p_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sc2337p_read(sd, 0x320d, &tmp);
	if (0 != ret) {
		ISP_ERROR("err: sc2337p read err\n");
		return ret;
	}
	hts = ((hts << 8) + tmp);
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	ret += sc2337p_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sc2337p_write(sd, 0x320e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("err: sc2337p_write err\n");
		return ret;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = vts - 6;
	sensor->video.attr->integration_time_limit = vts - 6;
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = vts - 6;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int sc2337p_set_mode(struct tx_isp_subdev *sd, int value)
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

static int sc2337p_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc2337p_reset");
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
		ret = private_gpio_request(pwdn_gpio,"sc2337p_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
    ret += sc2337p_write(sd, 0x301a, 0xf8);
    ret += sc2337p_write(sd, 0x0100, 0x01);
    private_msleep(5);
	ret = sc2337p_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc2337p chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("sc2337p chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "sc2337p", sizeof("sc2337p"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc2337p_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val;

	val = sc2337p_read(sd, 0x3221, &val);
	switch(enable) {
	case 0:
        val &= 0x99;
		break;
	case 1:
		printk("---mirror-----\n");
        val = ((val & 0x9F) | 0x06);
		break;
	case 2:
		printk("----flip---\n");
        val = ((val & 0xF9) | 0x60);
		break;
	case 3:
		printk("---mirror and flip-----\n");
        val = 0x66;
		break;
	}
	sc2337p_write(sd, 0x3221, val);

	return ret;
}

static int sc2337p_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}

	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
	     	ret = sc2337p_set_expo(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
	//	if(arg)
	//		ret = sc2337p_set_integration_time(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
	//	if(arg)
	//		ret = sc2337p_set_analog_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = sc2337p_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = sc2337p_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = sc2337p_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc2337p_write_array(sd, sc2337p_stream_off_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc2337p_write_array(sd, sc2337p_stream_on_mipi);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = sc2337p_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = sc2337p_set_vflip(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_LOGIC:
		if(arg)
			ret = sc2337p_set_logic(sd, *(int*)arg);
	default:
		break;
	}

	return ret;
}

static int sc2337p_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc2337p_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc2337p_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;

	sc2337p_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc2337p_core_ops = {
	.g_chip_ident = sc2337p_g_chip_ident,
	.reset = sc2337p_reset,
	.init = sc2337p_init,
	/*.ioctl = sc2337p_ops_ioctl,*/
	.g_register = sc2337p_g_register,
	.s_register = sc2337p_s_register,
};

static struct tx_isp_subdev_video_ops sc2337p_video_ops = {
	.s_stream = sc2337p_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc2337p_sensor_ops = {
	.ioctl	= sc2337p_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc2337p_ops = {
	.core = &sc2337p_core_ops,
	.video = &sc2337p_video_ops,
	.sensor = &sc2337p_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc2337p",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sensor_mclk_config(struct tx_isp_sensor *sensor, unsigned long want_rate)
{
        unsigned long rate = 0;
        struct clk *pll = NULL;
        char *plls[] = {"mpll", "sclka"};
        int psize = sizeof(plls) / sizeof(char *);
        char *ppll = plls[psize - 1];
        int ret = 0, i = 0;

        pll = clk_get_parent(sensor->mclk);
        rate = clk_get_rate(pll);
        if (rate % want_rate) {
                for (i = 0; i < psize; i++) {
                        pll = clk_get(NULL, plls[i]);
                        rate = clk_get_rate(pll);
                        if (!(rate % want_rate)) {
                                ret = clk_set_parent(sensor->mclk, pll);
                                if (ret) {
                                        ISP_WARNING("[%s %d] %s mounted node switchover failed !!!\n",
                                                    __func__, __LINE__, plls[i]);
                                        continue;
                                } else {
                                        break;
                                }
                        }
                }
                if (i == psize) {
                        if (!ret) {
                                pll = clk_get(NULL, ppll);
                                rate = clk_get_rate(pll);
                                if(want_rate == 37125000){
                                        if((rate >= 1188000000)) {
                                                rate = 1188000000;
                                        } else if (rate >= 891000000) {
                                                rate = 891000000;
                                        } else {
                                                ISP_ERROR("[%s %d] The %s clock setting failed !!!\n",
                                                          __func__, __LINE__, ppll);
                                                ret = -1;
                                                goto error;
                                        }
                                } else if (want_rate == 24000000 || want_rate == 27000000) {
                                        rate -= rate % want_rate;
                                } else {
                                        ret = -1;
                                        goto error;
                                }
                                ret = private_clk_set_rate(pll, rate);
                                if (ret) {
                                        ISP_WARNING("[%s %d] Failed to set %s !!!\n",
                                                    __func__, __LINE__, ppll);
                                        goto error;
                                } else {
                                        ISP_WARNING("[%s %d] !!!!!!!!!!! The %s frequency has been changed to %ld !!!\n",
                                                    __func__, __LINE__, ppll, rate);
                                }
                                ret = clk_set_parent(sensor->mclk, pll);
                                if (ret) {
                                        ISP_WARNING("[%s %d] %s mounted node switchover failed !!!\n",
                                                    __func__, __LINE__, ppll);
                                        goto error;
                                }
                        } else {
                                goto error;
                        }
                }
        }
        private_clk_set_rate(sensor->mclk, want_rate);
        private_clk_enable(sensor->mclk);

        rate = clk_get_rate(sensor->mclk);
        if (rate % want_rate) {
                ret = -1;
                goto error;
        }

        return ret;

error:
        ISP_ERROR("[%s %d] Unable to allocate the required MCLK %ld !!!\n",
                  __func__, __LINE__, want_rate);
        return ret;
}

static int sc2337p_probe(struct i2c_client *client, const struct i2c_device_id *id)
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
#ifdef CONFIG_KERNEL_4_4_94
	sensor->mclk = clk_get(NULL, "div_cim");
#else
	sensor->mclk = clk_get(NULL, "cgu_cim");
#endif
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	if(sensor_max_fps==30){
		sensor_mclk_config(sensor, 27000000);
		wsize = &sc2337p_win_sizes[1];
		memcpy((void*)(&(sc2337p_attr.mipi)),(void*)(&sc2337p_mipi_1lane),sizeof(sc2337p_mipi_1lane));
		sc2337p_attr.min_integration_time = 1,
		sc2337p_attr.min_integration_time_native = 1,
		sc2337p_attr.max_integration_time_native = 1125-6;
		sc2337p_attr.integration_time_limit = 1125-6;
		sc2337p_attr.total_width = 2200;
		sc2337p_attr.total_height = 1125;
		sc2337p_attr.max_integration_time = 1125-6;
		sc2337p_attr.one_line_expr_in_us = 25;
		printk("__sc2337p_win_sizes[1]__,\n");
	}else{
		sensor_mclk_config(sensor, 24000000);
		wsize = &sc2337p_win_sizes[0];
		memcpy((void*)(&(sc2337p_attr.mipi)),(void*)(&sc2337p_mipi_25fps),sizeof(sc2337p_mipi_25fps));
		sc2337p_attr.min_integration_time = 1,
		sc2337p_attr.min_integration_time_native = 1,
		sc2337p_attr.max_integration_time_native = 0X5a0-6;
		sc2337p_attr.integration_time_limit = 0X5a0-6;
		sc2337p_attr.total_width = 2200;
		sc2337p_attr.total_height = 0x5a0;
		sc2337p_attr.max_integration_time = 0X5a0-6;
		sc2337p_attr.one_line_expr_in_us = 25;
		printk("__sc2337p_win_sizes[0]__,\n");
	}

	/*
	  convert sensor-gain into isp-gain,
	*/
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sc2337p_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc2337p_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc2337p\n");

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int sc2337p_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc2337p_id[] = {
	{ "sc2337p", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc2337p_id);

static struct i2c_driver sc2337p_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sc2337p",
	},
	.probe		= sc2337p_probe,
	.remove		= sc2337p_remove,
	.id_table	= sc2337p_id,
};

static __init int init_sc2337p(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init sc2337p dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&sc2337p_driver);
}

static __exit void exit_sc2337p(void)
{
	private_i2c_del_driver(&sc2337p_driver);
}

module_init(init_sc2337p);
module_exit(exit_sc2337p);

MODULE_DESCRIPTION("A low-level driver for SmartSens sc2337p sensors");
MODULE_LICENSE("GPL");
