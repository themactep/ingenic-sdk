/*
 * sc233a.c
 *
 * Copyright (C) 2022 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * sboot        resolution      fps     interface              mode
 *   0          1920*1080       25        mipi_2lane           linear
 *   1          1920*1080       25        mipi_2lane           hdr
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

#define SC233A_CHIP_ID_H	(0xcb)
#define SC233A_CHIP_ID_L	(0x3e)
#define SC233A_REG_END		0xffff
#define SC233A_REG_DELAY	0xfffe
#define SENSOR_OUTPUT_MAX_FPS 25
#define SENSOR_OUTPUT_MAX_FPS_DOL 25
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20220105a"
#define MCLK 24000000

static int reset_gpio = -1;
static int pwdn_gpio = -1;
static int wdr_bufsize = 3840000;//1000*1920*2
static int shvflip = 1;
static unsigned char switch_wdr = 1;
static int data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;


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

struct again_lut sc233a_again_lut[] = {
	{0x80, 0},
	{0x84, 2886},
	{0x88, 5776},
	{0x8c, 8494},
	{0x90, 11136},
	{0x94, 13706},
	{0x98, 16287},
	{0x9c, 18723},
	{0xa0, 21097},
	{0xa4, 23414},
	{0xa8, 25746},
	{0xac, 27953},
	{0xb0, 30109},
	{0xb4, 32217},
	{0xb8, 34345},
	{0xbc, 36361},
	{0xc0, 38336},
	{0xc4, 40270},
	{0xc8, 42226},
	{0xcc, 44082},
	{0xd0, 45904},
	{0xd4, 47690},
	{0xd8, 49500},
	{0xdc, 51220},
	{0xe0, 52910},
	{0xe4, 54571},
	{0x4080, 56098},
	{0x4084, 58979},
	{0x4088, 61873},
	{0x408c, 64585},
	{0x4090, 67222},
	{0x4094, 69788},
	{0x4098, 72373},
	{0x409c, 74804},
	{0x40a0, 77175},
	{0x40a4, 79528},
	{0x40a8, 81863},
	{0x40ac, 84065},
	{0x40b0, 86216},
	{0x40b4, 88320},
	{0x40b8, 90451},
	{0x40bc, 92463},
	{0x40c0, 94434},
	{0x40c4, 96364},
	{0x40c8, 98323},
	{0x40cc, 100176},
	{0x40d0, 101994},
	{0x40d4, 103777},
	{0x40d8, 105589},
	{0x40dc, 107307},
	{0x40e0, 109023},
	{0x40e4, 110680},
	{0x40e8, 112366},
	{0x40ec, 113966},
	{0x40f0, 115539},
	{0x40f4, 117086},
	{0x40f8, 118662},
	{0x40fc, 120160},
	{0x4880, 121634},
	{0x4884, 124515},
	{0x4888, 127409},
	{0x488c, 130121},
	{0x4890, 132758},
	{0x4894, 135346},
	{0x4898, 137931},
	{0x489c, 140362},
	{0x48a0, 142732},
	{0x48a4, 145043},
	{0x48a8, 147379},
	{0x48ac, 149581},
	{0x48b0, 151733},
	{0x48b4, 153856},
	{0x48b8, 155987},
	{0x48bc, 157999},
	{0x48c0, 159970},
	{0x48c4, 161900},
	{0x48c8, 163859},
	{0x48cc, 165712},
	{0x48d0, 167530},
	{0x48d4, 169329},
	{0x48d8, 171141},
	{0x48dc, 172858},
	{0x48e0, 174544},
	{0x48e4, 176201},
	{0x48e8, 177888},
	{0x48ec, 179487},
	{0x48f0, 181061},
	{0x48f4, 182622},
	{0x48f8, 184198},
	{0x48fc, 185696},
	{0x4980, 187170},
	{0x4984, 190051},
	{0x4988, 192945},
	{0x498c, 195669},
	{0x4990, 198306},
	{0x4994, 200871},
	{0x4998, 203456},
	{0x499c, 205898},
	{0x49a0, 208268},
	{0x49a4, 210579},
	{0x49a8, 212915},
	{0x49ac, 215127},
	{0x49b0, 217279},
	{0x49b4, 219383},
	{0x49b8, 221514},
	{0x49bc, 223535},
	{0x49c0, 225506},
	{0x49c4, 227436},
	{0x49c8, 229395},
	{0x49cc, 231256},
	{0x49d0, 233074},
	{0x49d4, 234857},
	{0x49d8, 236669},
	{0x49dc, 238394},
	{0x49e0, 240080},
	{0x49e4, 241737},
	{0x49e8, 243424},
	{0x49ec, 245030},
	{0x49f0, 246604},
	{0x49f4, 248151},
	{0x49f8, 249727},
	{0x49fc, 251232},
	{0x4b80, 252706},
	{0x4b84, 255593},
	{0x4b88, 258481},
	{0x4b8c, 261199},
	{0x4b90, 263842},
	{0x4b94, 266413},
	{0x4b98, 268992},
	{0x4b9c, 271429},
	{0x4ba0, 273804},
	{0x4ba4, 276120},
	{0x4ba8, 278451},
	{0x4bac, 280658},
	{0x4bb0, 282815},
	{0x4bb4, 284923},
	{0x4bb8, 287050},
	{0x4bbc, 289067},
	{0x4bc0, 291042},
	{0x4bc4, 292976},
	{0x4bc8, 294931},
	{0x4bcc, 296788},
	{0x4bd0, 298610},
	{0x4bd4, 300397},
	{0x4bd8, 302205},
	{0x4bdc, 303926},
	{0x4be0, 305616},
	{0x4be4, 307277},
	{0x4be8, 308960},
	{0x4bec, 310563},
	{0x4bf0, 312140},
	{0x4bf4, 313690},
	{0x4bf8, 315263},
	{0x4bfc, 316764},
	{0x4f80, 318242},
	{0x4f84, 321129},
	{0x4f88, 324017},
	{0x4f8c, 326735},
	{0x4f90, 329378},
	{0x4f94, 331949},
	{0x4f98, 334528},
	{0x4f9c, 336965},
	{0x4fa0, 339340},
	{0x4fa4, 341656},
	{0x4fa8, 343987},
	{0x4fac, 346194},
	{0x4fb0, 348351},
	{0x4fb4, 350459},
	{0x4fb8, 352586},
	{0x4fbc, 354603},
	{0x4fc0, 356578},
	{0x4fc4, 358512},
	{0x4fc8, 360467},
	{0x4fcc, 362324},
	{0x4fd0, 364146},
	{0x4fd4, 365933},
	{0x4fd8, 367741},
	{0x4fdc, 369462},
	{0x4fe0, 371152},
	{0x4fe4, 372813},
	{0x4fe8, 374496},
	{0x4fec, 376099},
	{0x4ff0, 377676},
	{0x4ff4, 379226},
	{0x4ff8, 380799},
	{0x4ffc, 382300},
};

struct tx_isp_sensor_attribute sc233a_attr;

unsigned int sc233a_alloc_integration_time(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;
	*sensor_it = expo;

	return isp_it;
}
unsigned int sc233a_alloc_integration_time_short(unsigned int it, unsigned char shift, unsigned int *sensor_it)
{
	unsigned int expo = it >> shift;
	unsigned int isp_it = it;
	*sensor_it = expo;

	return isp_it;
}

unsigned int sc233a_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
        struct again_lut *lut = sc233a_again_lut;

        while(lut->gain <= sc233a_attr.max_again) {
                if(isp_gain == 0) {
                        *sensor_again = lut->value;
                        return 0;
                } else if(isp_gain < lut->gain) {
                        *sensor_again = (lut - 1)->value;
                        return (lut - 1)->gain;
                } else {
                        if((lut->gain == sc233a_attr.max_again) && (isp_gain >= lut->gain)) {
                                *sensor_again = lut->value;
                                return lut->gain;
                        }
                }

                lut++;
        }

        return isp_gain;
}
unsigned int sc233a_alloc_again_short(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
        struct again_lut *lut = sc233a_again_lut;

        while(lut->gain <= sc233a_attr.max_again) {
                if(isp_gain == 0) {
                        *sensor_again = lut->value;
                        return 0;
                } else if(isp_gain < lut->gain) {
                        *sensor_again = (lut - 1)->value;
                        return (lut - 1)->gain;
                } else{
                        if((lut->gain == sc233a_attr.max_again) && (isp_gain >= lut->gain)) {
                                *sensor_again = lut->value;
                                return lut->gain;
                        }
                }

                lut++;
        }

        return isp_gain;
}

unsigned int sc233a_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_mipi_bus sc233a_mipi_dol = {
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 660,
	.lans = 2,
	.settle_time_apative_en = 0,
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
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
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_WDR_2_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_VC_MODE,
};

struct tx_isp_mipi_bus sc233a_mipi_linear={
	.mode = SENSOR_MIPI_OTHER_MODE,
	.clk = 405,
	.lans = 2,
	.index = 0,
	.settle_time_apative_en = 0,
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
	.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
	.mipi_sc.hcrop_diff_en = 0,
	.mipi_sc.mipi_vcomp_en = 0,
	.mipi_sc.mipi_hcomp_en = 0,
	.mipi_sc.line_sync_mode = 0,
	.mipi_sc.work_start_flag = 0,
	.mipi_sc.data_type_en = 0,
	.mipi_sc.data_type_value = RAW10,
	.mipi_sc.del_start = 0,
	.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
	.mipi_sc.sensor_fid_mode = 0,
	.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
};

struct tx_isp_sensor_attribute sc233a_attr={
	.name = "sc233a",
	.chip_id = 0xcb3e,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = TISP_SBUS_MASK_SAMPLE_8BITS | TISP_SBUS_MASK_ADDR_16BITS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL,
	.cbus_device = 0x30,
	.max_again = 382300,
	.max_dgain = 0,
	.min_integration_time = 3,
	.min_integration_time_short = 5,
	.min_integration_time_native = 5,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = sc233a_alloc_again,
	.sensor_ctrl.alloc_again_short = sc233a_alloc_again_short,
	.sensor_ctrl.alloc_dgain = sc233a_alloc_dgain,
	.sensor_ctrl.alloc_integration_time_short = sc233a_alloc_integration_time_short,
};

static struct regval_list sc233a_init_regs_1920_1080_25fps_mipi[] = {
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x301f,0x09},
	{0x30b8,0x44},
	{0x320c,0x08},//2250  hts
	{0x320d,0xca},//
	{0x320e,0x05},//1440  vts
	{0x320f,0xa0},//
	{0x3253,0x0c},
	{0x3281,0x80},
	{0x3301,0x06},
	{0x3302,0x12},
	{0x3306,0x84},
	{0x3309,0x60},
	{0x330a,0x00},
	{0x330b,0xe0},
	{0x330d,0x20},
	{0x3314,0x15},
	{0x331e,0x41},
	{0x331f,0x51},
	{0x3320,0x0a},
	{0x3326,0x0e},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x335d,0x60},
	{0x335e,0x06},
	{0x335f,0x08},
	{0x3364,0x56},
	{0x337a,0x06},
	{0x337b,0x0e},
	{0x337c,0x02},
	{0x337d,0x0a},
	{0x3390,0x03},
	{0x3391,0x0f},
	{0x3392,0x1f},
	{0x3393,0x06},
	{0x3394,0x06},
	{0x3395,0x06},
	{0x3396,0x48},
	{0x3397,0x4b},
	{0x3398,0x5f},
	{0x3399,0x06},
	{0x339a,0x06},
	{0x339b,0x9c},
	{0x339c,0x9c},
	{0x33a2,0x04},
	{0x33a3,0x0a},
	{0x33ad,0x1c},
	{0x33af,0x40},
	{0x33b1,0x80},
	{0x33b3,0x20},
	{0x349f,0x02},
	{0x34a6,0x48},
	{0x34a7,0x4b},
	{0x34a8,0x20},
	{0x34a9,0x20},
	{0x34f8,0x5f},
	{0x34f9,0x10},
	{0x3616,0xac},
	{0x3630,0xc0},
	{0x3631,0x86},
	{0x3632,0x26},
	{0x3633,0x32},
	{0x3637,0x29},
	{0x363a,0x84},
	{0x363b,0x04},
	{0x363c,0x08},
	{0x3641,0x3a},
	{0x364f,0x39},
	{0x3670,0xce},
	{0x3674,0xc0},
	{0x3675,0xc0},
	{0x3676,0xc0},
	{0x3677,0x86},
	{0x3678,0x8b},
	{0x3679,0x8c},
	{0x367c,0x4b},
	{0x367d,0x5f},
	{0x367e,0x4b},
	{0x367f,0x5f},
	{0x3690,0x62},
	{0x3691,0x63},
	{0x3692,0x63},
	{0x3699,0x86},
	{0x369a,0x92},
	{0x369b,0xa4},
	{0x369c,0x48},
	{0x369d,0x4b},
	{0x36a2,0x4b},
	{0x36a3,0x4f},
	{0x36ea,0x09},
	{0x36eb,0x0c},
	{0x36ec,0x1c},
	{0x36ed,0x28},
	{0x370f,0x01},
	{0x3721,0x6c},
	{0x3722,0x09},
	{0x3724,0x41},
	{0x3725,0xc4},
	{0x37b0,0x09},
	{0x37b1,0x09},
	{0x37b2,0x09},
	{0x37b3,0x48},
	{0x37b4,0x5f},
	{0x37fa,0x09},
	{0x37fb,0x32},
	{0x37fc,0x10},
	{0x37fd,0x37},
	{0x3900,0x19},
	{0x3901,0x02},
	{0x3905,0xb8},
	{0x391b,0x82},
	{0x391c,0x00},
	{0x391f,0x04},
	{0x3933,0x81},
	{0x3934,0x4c},
	{0x393f,0xff},
	{0x3940,0x73},
	{0x3942,0x01},
	{0x3943,0x4d},
	{0x3946,0x20},
	{0x3957,0x86},
	{0x3e01,0x95},
	{0x3e02,0x60},
	{0x3e28,0xc4},
	{0x440e,0x02},
	{0x4501,0xc0},
	{0x4509,0x14},
	{0x450d,0x11},
	{0x4518,0x00},
	{0x451b,0x0a},
	{0x4819,0x07},
	{0x481b,0x04},
	{0x481d,0x0e},
	{0x481f,0x03},
	{0x4821,0x09},
	{0x4823,0x04},
	{0x4825,0x03},
	{0x4827,0x03},
	{0x4829,0x06},
	{0x501c,0x00},
	{0x501d,0x60},
	{0x501e,0x00},
	{0x501f,0x40},
	{0x5799,0x06},
	{0x5ae0,0xfe},
	{0x5ae1,0x40},
	{0x5ae2,0x38},
	{0x5ae3,0x30},
	{0x5ae4,0x28},
	{0x5ae5,0x38},
	{0x5ae6,0x30},
	{0x5ae7,0x28},
	{0x5ae8,0x3f},
	{0x5ae9,0x34},
	{0x5aea,0x2c},
	{0x5aeb,0x3f},
	{0x5aec,0x34},
	{0x5aed,0x2c},
	{0x5aee,0xfe},
	{0x5aef,0x40},
	{0x5af4,0x38},
	{0x5af5,0x30},
	{0x5af6,0x28},
	{0x5af7,0x38},
	{0x5af8,0x30},
	{0x5af9,0x28},
	{0x5afa,0x3f},
	{0x5afb,0x34},
	{0x5afc,0x2c},
	{0x5afd,0x3f},
	{0x5afe,0x34},
	{0x5aff,0x2c},
	{0x36e9,0x53},
	{0x37f9,0x53},
	{0x0100,0x01},
	{SC233A_REG_END, 0x00},
};

static struct regval_list sc233a_init_regs_1920_1080_25fps_mipi_dol[] = {
	{0x0100,0x00},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x301f,0x65},
	{0x3200,0x00},
	{0x3201,0x00},
	{0x3202,0x00},
	{0x3203,0x00},
	{0x3204,0x07},
	{0x3205,0x87},
	{0x3206,0x04},
	{0x3207,0x3f},
	{0x3208,0x07},
	{0x3209,0x80},
	{0x320a,0x04},
	{0x320b,0x38},
	{0x320c,0x08},//2200  hts
	{0x320d,0x98},//
	{0x320e,0x09},//2400  vts
	{0x320f,0x60},//
	{0x3210,0x00},
	{0x3211,0x04},
	{0x3212,0x00},
	{0x3213,0x04},
	{0x3241,0x0a},
	{0x3243,0x0d},
	{0x3248,0x02},
	{0x3249,0x0b},
	{0x3250,0xff},
	{0x3253,0x0c},
	{0x3281,0x01},
	{0x3301,0x07},
	{0x3302,0x12},
	{0x3306,0x90},
	{0x3308,0x20},
	{0x3309,0x80},
	{0x330a,0x01},
	{0x330b,0x00},
	{0x330d,0x20},
	{0x330e,0x3c},
	{0x3314,0x14},
	{0x331e,0x41},
	{0x331f,0x71},
	{0x3320,0x0a},
	{0x3326,0x0e},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x3347,0x05},
	{0x335d,0x60},
	{0x335e,0x06},
	{0x335f,0x08},
	{0x3364,0x56},
	{0x336c,0xcf},
	{0x337a,0x06},
	{0x337b,0x0e},
	{0x337c,0x02},
	{0x337d,0x0a},
	{0x3390,0x03},
	{0x3391,0x0f},
	{0x3392,0x1f},
	{0x3393,0x07},
	{0x3394,0x07},
	{0x3395,0x07},
	{0x3396,0x49},
	{0x3397,0x4b},
	{0x3398,0x5f},
	{0x3399,0x0a},
	{0x339a,0x0a},
	{0x339b,0x28},
	{0x339c,0x48},
	{0x33a2,0x04},
	{0x33a3,0x0a},
	{0x33ad,0x3c},
	{0x33ae,0x13},
	{0x33af,0x40},
	{0x33b1,0x80},
	{0x33b2,0x40},
	{0x33b3,0x20},
	{0x3400,0x14},
	{0x3401,0x85},
	{0x349f,0x02},
	{0x34a6,0x40},
	{0x34a7,0x4b},
	{0x34a8,0x20},
	{0x34a9,0x08},
	{0x34f8,0x5f},
	{0x34f9,0x00},
	{0x3630,0x90},
	{0x3631,0x82},
	{0x3632,0x26},
	{0x3633,0x33},
	{0x3637,0x29},
	{0x363a,0x84},
	{0x363b,0x02},
	{0x363c,0x08},
	{0x3641,0x3a},
	{0x3670,0x4e},
	{0x3674,0x90},
	{0x3675,0x90},
	{0x3676,0x90},
	{0x3677,0x82},
	{0x3678,0x88},
	{0x3679,0x89},
	{0x367c,0x4b},
	{0x367d,0x5f},
	{0x367e,0x4b},
	{0x367f,0x5f},
	{0x3690,0x63},
	{0x3691,0x64},
	{0x3692,0x75},
	{0x3699,0x86},
	{0x369a,0x92},
	{0x369b,0xa4},
	{0x369c,0x40},
	{0x369d,0x4b},
	{0x36a2,0x4b},
	{0x36a3,0x4f},
	{0x36ea,0x0b},
	{0x36eb,0x04},
	{0x36ec,0x0c},
	{0x36ed,0x28},
	{0x370f,0x01},
	{0x3721,0x6c},
	{0x3722,0x09},
	{0x3724,0x41},
	{0x3725,0xc4},
	{0x37b0,0x09},
	{0x37b1,0x09},
	{0x37b2,0x09},
	{0x37b3,0x48},
	{0x37b4,0x5f},
	{0x37fa,0x0b},
	{0x37fb,0x04},
	{0x37fc,0x00},
	{0x37fd,0x27},
	{0x3900,0x19},
	{0x3901,0x02},
	{0x3905,0xb8},
	{0x391b,0x81},
	{0x391c,0x00},
	{0x391d,0x81},
	{0x391f,0x04},
	{0x3933,0x81},
	{0x3934,0x4c},
	{0x393f,0xff},
	{0x3940,0x70},
	{0x3942,0x01},
	{0x3943,0x4d},
	{0x3957,0x86},
	{0x3e00,0x01},
	{0x3e01,0x18},
	{0x3e02,0x00},
	{0x3e04,0x21},
	{0x3e05,0x80},
	{0x3e06,0x00},
	{0x3e07,0x80},
	{0x3e09,0x00},
	{0x3e10,0x00},
	{0x3e11,0x80},
	{0x3e13,0x00},
	{0x3e23,0x00},
	{0x3e24,0x92},
	{0x440e,0x02},
	{0x4509,0x14},
	{0x450d,0x11},
	{0x4518,0x00},
	{0x4816,0x71},
	{0x4819,0x09},
	{0x481b,0x05},
	{0x481d,0x14},
	{0x481f,0x04},
	{0x4821,0x0a},
	{0x4823,0x05},
	{0x4825,0x04},
	{0x4827,0x05},
	{0x4829,0x08},
	{0x4837,0x1e},
	{0x5010,0x00},
	{0x5799,0x06},
	{0x5ae0,0xfe},
	{0x5ae1,0x40},
	{0x5ae2,0x38},
	{0x5ae3,0x30},
	{0x5ae4,0x28},
	{0x5ae5,0x38},
	{0x5ae6,0x30},
	{0x5ae7,0x28},
	{0x5ae8,0x3f},
	{0x5ae9,0x34},
	{0x5aea,0x2c},
	{0x5aeb,0x3f},
	{0x5aec,0x34},
	{0x5aed,0x2c},
	{0x5aee,0xfe},
	{0x5aef,0x40},
	{0x5af4,0x38},
	{0x5af5,0x30},
	{0x5af6,0x28},
	{0x5af7,0x38},
	{0x5af8,0x30},
	{0x5af9,0x28},
	{0x5afa,0x3f},
	{0x5afb,0x34},
	{0x5afc,0x2c},
	{0x5afd,0x3f},
	{0x5afe,0x34},
	{0x5aff,0x2c},
	{0x36e9,0x20},
	{0x37f9,0x20},
	{0x0100,0x01},
	{SC233A_REG_END, 0x00},
};

/*
 * the order of the jxf23_win_sizes is [full_resolution, preview_resolution].
 */
static struct tx_isp_sensor_win_setting sc233a_win_sizes[] = {
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGR10_1X10,
		.colorspace	= TISP_COLORSPACE_SRGB,
		.regs 		= sc233a_init_regs_1920_1080_25fps_mipi,
	},
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 25 << 16 | 1,
		.mbus_code	= TISP_VI_FMT_SBGGR10_1X10,
		.mbus_code	= TISP_COLORSPACE_SRGB,
		.regs		= sc233a_init_regs_1920_1080_25fps_mipi_dol,
	}
};

struct tx_isp_sensor_win_setting *wsize = &sc233a_win_sizes[0];

/*
 * the part of driver was fixed.
 */

static struct regval_list sc233a_stream_on[] = {
	{SC233A_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc233a_stream_off[] = {
	{SC233A_REG_END, 0x00},	/* END MARKER */
};

int sc233a_read(struct tx_isp_subdev *sd,  uint16_t reg,
		unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[2] = {(reg>>8)&0xff, reg&0xff};
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

int sc233a_write(struct tx_isp_subdev *sd, uint16_t reg,
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
static int sc233a_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SC233A_REG_END) {
		if (vals->reg_num == SC233A_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc233a_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		pr_debug("vals->reg_num:0x%x, vals->value:0x%02x\n",vals->reg_num, val);
		vals++;
	}
	return 0;
}
#endif

static int sc233a_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SC233A_REG_END) {
		if (vals->reg_num == SC233A_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc233a_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc233a_reset(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	return 0;
}

#if 1
static int sc233a_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	unsigned char v;
	int ret;

	ret = sc233a_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC233A_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc233a_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;

	if (v != SC233A_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}
#endif

static int sc233a_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = (value & 0xffff );
	int again = (value & 0xffff0000) >> 16;
	if (data_type == TX_SENSOR_DATA_TYPE_WDR_DOL) it = (it << 1) + 1;
	ret = sc233a_write(sd,  0x3e00, (unsigned char)((it >> 12) & 0x0f));
	ret += sc233a_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc233a_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));


	ret += sc233a_write(sd, 0x3e09, (unsigned char)((again >> 8) & 0xff));
	ret += sc233a_write(sd, 0x3e07, (unsigned char)(again & 0xff));

	if (ret < 0)
		return ret;

	return 0;
}

#if 0
static int sc233a_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = 0;

	if (info->default_boot == 1){
		if(value > 1505) value = 1504;
	}

	ret = sc233a_write(sd, 0x0203, value & 0xff);
	ret += sc233a_write(sd, 0x0202, value >> 8);
	if (ret < 0) {
		ISP_ERROR("sc233a_write error  %d\n" ,__LINE__ );
		return ret;
	}

	return 0;
}
#endif

static int sc233a_set_integration_time_short(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

//	printk("\n==============> short_time = 0x%x\n", value);
	ret = sc233a_write(sd,  0x3e04, (unsigned char)((value >> 4) & 0xff));
	ret = sc233a_write(sd,  0x3e05, (unsigned char)(value & 0x0f) << 4);

	if (ret < 0)
		return ret;

	return 0;
}

#if 0
static int sc233a_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	struct again_lut *val_lut = sc233a_again_lut;


	ret = sc233a_write(sd, 0x02b3, val_lut[value].reg2b3);
	ret = sc233a_write(sd, 0x02b4, val_lut[value].reg2b4);
	ret = sc233a_write(sd, 0x02b8, val_lut[value].reg2b8);
	ret = sc233a_write(sd, 0x02b9, val_lut[value].reg2b9);
	ret = sc233a_write(sd, 0x0515, val_lut[value].reg515);
	ret = sc233a_write(sd, 0x0519, val_lut[value].reg519);
	ret = sc233a_write(sd, 0x02d9, val_lut[value].reg2d9);

	if (ret < 0) {
		ISP_ERROR("sc233a_write error  %d" ,__LINE__ );
		return ret;
	}

	return 0;
}
#endif

static int sc233a_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc233a_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc233a_init(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
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

static int sc233a_s_stream(struct tx_isp_subdev *sd, struct tx_isp_initarg *init)
{
	int ret = 0;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	if (init->enable) {
		if (sensor->video.state == TX_ISP_MODULE_DEINIT){
			ret = sc233a_write_array(sd, wsize->regs);
			if (ret)
				return ret;
			sensor->video.state = TX_ISP_MODULE_INIT;
		}
		if (sensor->video.state == TX_ISP_MODULE_INIT) {
			ret = sc233a_write_array(sd, sc233a_stream_on);
			sensor->video.state = TX_ISP_MODULE_RUNNING;
			pr_debug("sc233a stream on\n");
			sensor->video.state = TX_ISP_MODULE_RUNNING;
		}
	} else {
		ret = sc233a_write_array(sd, sc233a_stream_off);
		pr_debug("sc233a stream off\n");
	}

	return ret;
}

static int sc233a_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned short vts = 0;
	unsigned short hts=0;
	unsigned int sensor_max_fps;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;
	unsigned int short_time;
	unsigned char val;

	switch(sensor->info.default_boot){
	case 0:
		sclk = 1440 * 2250 * 25 * 2;
		sensor_max_fps = TX_SENSOR_MAX_FPS_25;
		break;
	case 1:
		sclk = 2200 * 2400 * 25 * 2;
		sensor_max_fps = TX_SENSOR_MAX_FPS_25;
		break;
	default:
		ISP_ERROR("Now we do not support this framerate!!!\n");
	}

	/* the format of fps is 16/16. for example 30 << 16 | 2, the value is 30/2 fps. */
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (sensor_max_fps << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)){
		ISP_ERROR("warn: fps(%x) no in range\n", fps);
		return -1;
	}

	val = 0;
	ret += sc233a_read(sd, 0x320c, &val);
	hts = val<<8;
	val = 0;
	ret += sc233a_read(sd, 0x320d, &val);
	hts |= val;
	hts *= 2;

	if (0 != ret) {
		ISP_ERROR("err: sc233a read err\n");
		return ret;
	}

	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);

	sc233a_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	sc233a_write(sd, 0x320e, (unsigned char)(vts >> 8));

	if(sensor->info.default_boot != 1){
	sc233a_read(sd, 0x3e23, &val);
	short_time = val << 8;
	sc233a_read(sd, 0x3e24, &val);
	short_time |= val;
	short_time *= 2;
	}

	if (0 != ret) {
		ISP_ERROR("err: sc233a_write err\n");
		return ret;
	}

	sensor->video.fps = fps;
	sensor->video.attr->max_integration_time_native = (sensor->info.default_boot == 1) ? (2*vts -10) : (2*vts - short_time - 18);
	sensor->video.attr->integration_time_limit = (sensor->info.default_boot == 1) ? (2*vts -10) : (2*vts - short_time - 18);
	sensor->video.attr->total_height = vts;
	sensor->video.attr->max_integration_time = (sensor->info.default_boot == 1) ? (2*vts -10) : (2*vts - short_time - 18);

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	if (ret) {
		ISP_WARNING("Description Failed to synchronize the attributes of sensor!!!");
	}

	return ret;
}


static int sc233a_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	/* 2'b01:mirror,2'b10:filp */
	val = sc233a_read(sd, 0x3221, &val);
	switch(enable) {
	case 0:
		sc233a_write(sd, 0x3221, val & 0x99);
		break;
	case 1:
		sc233a_write(sd, 0x3221, val | 0x06);
		break;
	case 2:
		sc233a_write(sd, 0x3221, val | 0x60);
		break;
	case 3:
		sc233a_write(sd, 0x3221, val | 0x66);
		break;
	}

	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}


static int sc233a_set_mode(struct tx_isp_subdev *sd, int value)
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

struct clk *sclka;
static int sensor_attr_check(struct tx_isp_subdev *sd)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned long rate;
	int ret = 0;

	switch(info->default_boot){
	case 0:
		wsize = &sc233a_win_sizes[0];
		memcpy(&sc233a_attr.mipi, &sc233a_mipi_linear, sizeof(sc233a_mipi_linear));
		sc233a_attr.min_integration_time = 3;
		sc233a_attr.total_width = 2250;
		sc233a_attr.total_height = 1440;
		sc233a_attr.max_integration_time_native = 2870;   /* 1440*2-10 */
		sc233a_attr.integration_time_limit = 2870;
		sc233a_attr.max_integration_time = 2870;
		sc233a_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		sc233a_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		printk("=================> linear is ok");
		break;
	case 1:
		sc233a_attr.wdr_cache = wdr_bufsize;
		wsize = &sc233a_win_sizes[1];
		memcpy(&sc233a_attr.mipi, &sc233a_mipi_dol, sizeof(sc233a_mipi_dol));
		sc233a_attr.mipi.clk = 660,
		sc233a_attr.min_integration_time = 3;
		sc233a_attr.min_integration_time_short = 3;
		sc233a_attr.total_width = 2200;
		sc233a_attr.total_height = 2400;
		sc233a_attr.max_integration_time_native = 2247; /* 2400-146-7 */
		sc233a_attr.integration_time_limit = 2247;
		sc233a_attr.max_integration_time = 2247;
		sc233a_attr.max_integration_time_short = 140; /* 146-6 */
		sc233a_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		printk("=================> 25fps_hdr is ok");
		break;
	default:
		ISP_ERROR("Have no this setting!!!\n");
	}

	data_type = sc233a_attr.data_type;

	switch(info->video_interface){
	case TISP_SENSOR_VI_MIPI_CSI0:
		sc233a_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc233a_attr.mipi.index = 0;
		break;
	case TISP_SENSOR_VI_MIPI_CSI1:
		sc233a_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI;
		sc233a_attr.mipi.index = 1;
		break;
	case TISP_SENSOR_VI_DVP:
		sc233a_attr.dbus_type = TX_SENSOR_DATA_INTERFACE_DVP;
		break;
	default:
		ISP_ERROR("Have no this interface!!!\n");
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

	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	rate = private_clk_get_rate(sensor->mclk);

	if (((rate / 1000) % 24000) != 0) {
                ret = clk_set_parent(sclka, clk_get(NULL, SEN_TCLK));
                sclka = private_devm_clk_get(&client->dev, SEN_TCLK);
		if (IS_ERR(sclka)) {
			pr_err("get sclka failed\n");
		} else {
			rate = private_clk_get_rate(sclka);
			if (((rate / 1000) % 24000) != 0) {
				private_clk_set_rate(sclka, 1200000000);
			}
		}
	}

        private_clk_set_rate(sensor->mclk, MCLK);
        private_clk_prepare_enable(sensor->mclk);

	reset_gpio = info->rst_gpio;
	pwdn_gpio = info->pwdn_gpio;
        sensor->video.max_fps = wsize->fps;
	sensor->video.min_fps = SENSOR_OUTPUT_MIN_FPS << 16 | 1;
	return 0;

err_get_mclk:
	return -1;
}

static int sc233a_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;

	sensor_attr_check(sd);
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc233a_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(20);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"sc233a_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sc233a_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc233a chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("sc233a chip found @ 0x%02x (%s)\n sensor drv version %s", client->addr, client->adapter->name, SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "sc233a", sizeof("sc233a"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

#if 1
static int sc233a_set_wdr_stop(struct tx_isp_subdev *sd, int wdr_en)
{
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);
	struct tx_isp_sensor_register_info *info = &sensor->info;
	int ret = 0;

	//ret = sc233a_write(sd, 0x0103, 0x1);

	if(wdr_en == 1){
		info->default_boot=1;
		memcpy(&sc233a_attr.mipi, &sc233a_mipi_dol, sizeof(sc233a_mipi_dol));
		sc233a_attr.data_type = TX_SENSOR_DATA_TYPE_WDR_DOL;
		sc233a_attr.wdr_cache = wdr_bufsize;
		wsize = &sc233a_win_sizes[1];
		sc233a_attr.mipi.clk = 660,
		sc233a_attr.min_integration_time = 3;
		sc233a_attr.min_integration_time_short = 3;
		sc233a_attr.total_width = 2200;
		sc233a_attr.total_height = 2400;
		sc233a_attr.max_integration_time_native = 2247;
		sc233a_attr.integration_time_limit = 2247;
		sc233a_attr.max_integration_time = 2247;
		sc233a_attr.max_integration_time_short = 140;
		printk("\n-------------------------switch wdr@25fps ok ----------------------\n");
	}else if (wdr_en == 0){
		switch_wdr = info->default_boot;
		info->default_boot = 0;
		memcpy(&sc233a_attr.mipi, &sc233a_mipi_linear, sizeof(sc233a_mipi_linear));
		sc233a_attr.data_type = TX_SENSOR_DATA_TYPE_LINEAR;
		wsize = &sc233a_win_sizes[0];
		sc233a_attr.min_integration_time = 3;
		sc233a_attr.total_width = 2250;
		sc233a_attr.total_height = 1440;
		sc233a_attr.max_integration_time_native = 2870;
		sc233a_attr.integration_time_limit = 2870;
		sc233a_attr.max_integration_time = 2870;
		printk("\n-------------------------switch linear ok ----------------------\n");
	}else{
		ISP_ERROR("Can not support this data type!!!");
		return -1;
	}

	data_type = sc233a_attr.data_type;

	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int sc233a_set_wdr(struct tx_isp_subdev *sd, int wdr_en)
{
	int ret = 0;
//	printk("\n==========> set_wdr\n");
	private_gpio_direction_output(reset_gpio, 1);
	private_msleep(1);
	private_gpio_direction_output(reset_gpio, 0);
	private_msleep(1);
	private_gpio_direction_output(reset_gpio, 1);
	private_msleep(1);

	ret = sc233a_write_array(sd, wsize->regs);
	ret = sc233a_write_array(sd, sc233a_stream_on);

	return 0;
}
#endif

static int sc233a_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	struct tx_isp_sensor_value *sensor_val = arg;
	struct tx_isp_initarg *init = arg;

	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = sc233a_set_expo(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
	//	if(arg)
	//		ret = sc233a_set_integration_time(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_AGAIN:
	//	if(arg)
	//		ret = sc233a_set_analog_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = sc233a_set_digital_gain(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = sc233a_get_black_pedestal(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = sc233a_set_mode(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = sc233a_write_array(sd, sc233a_stream_off);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = sc233a_write_array(sd, sc233a_stream_on);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = sc233a_set_fps(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME_SHORT:
		if(arg)
			ret = sc233a_set_integration_time_short(sd, sensor_val->value);
		break;
	case TX_ISP_EVENT_SENSOR_WDR:
		if(arg)
			ret = sc233a_set_wdr(sd, init->enable);
		break;
	case TX_ISP_EVENT_SENSOR_WDR_STOP:
		if(arg)
			ret = sc233a_set_wdr_stop(sd, init->enable);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = sc233a_set_vflip(sd, sensor_val->value);
		break;
	default:
		break;
	}

	return ret;
}

static int sc233a_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc233a_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc233a_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	sc233a_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc233a_core_ops = {
	.g_chip_ident = sc233a_g_chip_ident,
	.reset = sc233a_reset,
	.init = sc233a_init,
	/*.ioctl = sc233a_ops_ioctl,*/
	.g_register = sc233a_g_register,
	.s_register = sc233a_s_register,
};

static struct tx_isp_subdev_video_ops sc233a_video_ops = {
	.s_stream = sc233a_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc233a_sensor_ops = {
	.ioctl	= sc233a_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc233a_ops = {
	.core = &sc233a_core_ops,
	.video = &sc233a_video_ops,
	.sensor = &sc233a_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc233a",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sc233a_probe(struct i2c_client *client, const struct i2c_device_id *id)
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
	sensor->dev = &client->dev;
	sd = &sensor->sd;
	video = &sensor->video;
	sc233a_attr.expo_fs = 1;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &sc233a_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = TISP_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc233a_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc233a\n");

	return 0;
}

static int sc233a_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if(reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if(pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable_unprepare(sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id sc233a_id[] = {
	{ "sc233a", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc233a_id);

static struct i2c_driver sc233a_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sc233a",
	},
	.probe		= sc233a_probe,
	.remove		= sc233a_remove,
	.id_table	= sc233a_id,
};

static __init int init_sc233a(void)
{
	return private_i2c_add_driver(&sc233a_driver);
}

static __exit void exit_sc233a(void)
{
	private_i2c_del_driver(&sc233a_driver);
}

module_init(init_sc233a);
module_exit(exit_sc233a);

MODULE_DESCRIPTION("A low-level driver for sc233a sensors");
MODULE_LICENSE("GPL");
