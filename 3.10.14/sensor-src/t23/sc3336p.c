/*sc3336p.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Settings:
 * default_boot    resolution      fps       interface          mode        DVDD
 *       0         2304*1296       45        mipi_2lane        linear       external=1.5v
 *       1         2304*1296       30        mipi_2lane        linear       interior
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

#define SC3336P_CHIP_ID_H	(0x9c)
#define SC3336P_CHIP_ID_L	(0x41)
#define SC3336P_REG_END		0xffff
#define SC3336P_REG_DELAY	0xfffe
#define SENSOR_OUTPUT_MAX_FPS 45
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20250206a"

static int reset_gpio = GPIO_PA(18);
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int data_interface = TX_SENSOR_DATA_INTERFACE_MIPI;
module_param(data_interface, int, S_IRUGO);
MODULE_PARM_DESC(data_interface, "Sensor Date interface");

static int default_boot = 1;
module_param(default_boot, int, S_IRUGO);
MODULE_PARM_DESC(default_boot, "Sensor Max default boot interface");

static int shvflip = 1;
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

struct again_lut sc3336p_again_lut[] = {
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
	{0x4080, 39588},
	{0x4084, 42467},
	{0x4088, 45378},
	{0x408c, 48089},
	{0x4090, 50724},
	{0x4094, 53288},
	{0x4098, 55888},
	{0x409c, 58318},
	{0x40a0, 60686},
	{0x40a4, 62996},
	{0x40a8, 65346},
	{0x40ac, 67547},
	{0x40b0, 69697},
	{0x40b4, 71800},
	{0x40b8, 73943},
	{0x40bc, 75955},
	{0x40c0, 77924},
	{0x40c4, 79853},
	{0x40c8, 81823},
	{0x40cc, 83676},
	{0x40d0, 85492},
	{0x40d4, 87274},
	{0x40d8, 89097},
	{0x40dc, 90813},
	{0x40e0, 92499},
	{0x40e4, 94155},
	{0x40e8, 95851},
	{0x40ec, 97450},
	{0x40f0, 99022},
	{0x40f4, 100568},
	{0x40f8, 102154},
	{0x40fc, 103651},
	{0x4880, 105124},
	{0x4884, 108003},
	{0x4888, 110914},
	{0x488c, 113625},
	{0x4890, 116260},
	{0x4894, 118824},
	{0x4898, 121424},
	{0x489c, 123854},
	{0x48a0, 126222},
	{0x48a4, 128532},
	{0x48a8, 130882},
	{0x48ac, 133083},
	{0x48b0, 135233},
	{0x48b4, 137336},
	{0x48b8, 139479},
	{0x48bc, 141491},
	{0x48c0, 143460},
	{0x48c4, 145389},
	{0x48c8, 147359},
	{0x48cc, 149212},
	{0x48d0, 151028},
	{0x48d4, 152810},
	{0x48d8, 154633},
	{0x48dc, 156349},
	{0x48e0, 158035},
	{0x48e4, 159691},
	{0x48e8, 161387},
	{0x48ec, 162986},
	{0x48f0, 164558},
	{0x48f4, 166104},
	{0x48f8, 167690},
	{0x48fc, 169187},
	{0x4980, 170660},
	{0x4984, 173539},
	{0x4988, 176436},
	{0x498c, 179161},
	{0x4990, 181796},
	{0x4994, 184360},
	{0x4998, 186947},
	{0x499c, 189390},
	{0x49a0, 191758},
	{0x49a4, 194068},
	{0x49a8, 196406},
	{0x49ac, 198619},
	{0x49b0, 200769},
	{0x49b4, 202872},
	{0x49b8, 205005},
	{0x49bc, 207027},
	{0x49c0, 208996},
	{0x49c4, 210925},
	{0x49c8, 212886},
	{0x49cc, 214748},
	{0x49d0, 216564},
	{0x49d4, 218346},
	{0x49d8, 220160},
	{0x49dc, 221885},
	{0x49e0, 223571},
	{0x49e4, 225227},
	{0x49e8, 226914},
	{0x49ec, 228522},
	{0x49f0, 230094},
	{0x49f4, 231640},
	{0x49f8, 233218},
	{0x49fc, 234723},
	{0x4b80, 236196},
	{0x4b84, 239083},
	{0x4b88, 241972},
	{0x4b8c, 244690},
	{0x4b90, 247332},
	{0x4b94, 249902},
	{0x4b98, 252483},
	{0x4b9c, 254919},
	{0x4ba0, 257294},
	{0x4ba4, 259610},
	{0x4ba8, 261942},
	{0x4bac, 264149},
	{0x4bb0, 266305},
	{0x4bb4, 268413},
	{0x4bb8, 270541},
	{0x4bbc, 272557},
	{0x4bc0, 274532},
	{0x4bc4, 276466},
	{0x4bc8, 278422},
	{0x4bcc, 280279},
	{0x4bd0, 282100},
	{0x4bd4, 283887},
	{0x4bd8, 285696},
	{0x4bdc, 287417},
	{0x4be0, 289107},
	{0x4be4, 290767},
	{0x4be8, 292450},
	{0x4bec, 294053},
	{0x4bf0, 295630},
	{0x4bf4, 297181},
	{0x4bf8, 298754},
	{0x4bfc, 300255},
	{0x4f80, 301732},
	{0x4f84, 304619},
	{0x4f88, 307508},
	{0x4f8c, 310226},
	{0x4f90, 312868},
	{0x4f94, 315438},
	{0x4f98, 318019},
	{0x4f9c, 320455},
	{0x4fa0, 322830},
	{0x4fa4, 325146},
	{0x4fa8, 327478},
	{0x4fac, 329685},
	{0x4fb0, 331841},
	{0x4fb4, 333949},
	{0x4fb8, 336077},
	{0x4fbc, 338093},
	{0x4fc0, 340068},
	{0x4fc4, 342002},
	{0x4fc8, 343958},
	{0x4fcc, 345815},
	{0x4fd0, 347636},
	{0x4fd4, 349423},
	{0x4fd8, 351232},
	{0x4fdc, 352953},
	{0x4fe0, 354643},
	{0x4fe4, 356303},
	{0x4fe8, 357986},
	{0x4fec, 359589},
	{0x4ff0, 361166},
	{0x4ff4, 362717},
	{0x4ff8, 364290},
	{0x4ffc, 365791},
};

struct tx_isp_sensor_attribute sc3336p_attr;

unsigned int sc3336p_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = sc3336p_again_lut;
	while(lut->gain <= sc3336p_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == sc3336p_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int sc3336p_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute sc3336p_attr={
	.name = "sc3336p",
	.chip_id = 0x9c41,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = 0x30,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 743,
		.lans = 2,
		.settle_time_apative_en = 0,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,//RAW10
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.image_twidth = 2304,
		.image_theight = 1296,
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
	.max_again = 365791,
	.max_dgain = 0,
	.min_integration_time = 2,
	.min_integration_time_native = 2,
	.max_integration_time_native = 1320 - 8,
	.integration_time_limit = 1320 - 8,
	.total_width = 1250,
	.total_height = 1320,
	.max_integration_time = 1320 - 8,
	.one_line_expr_in_us = 29,
	.integration_time_apply_delay = 2,
	.again_apply_delay = 2,
	.dgain_apply_delay = 2,
	.sensor_ctrl.alloc_again = sc3336p_alloc_again,
	.sensor_ctrl.alloc_dgain = sc3336p_alloc_dgain,
};

static struct regval_list sc3336p_init_regs_2304_1296_45fps_mipi[] = {
	{0x0103,0x01},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x300a,0x24},
	{0x301f,0x11},
	{0x3032,0xa0},
	{0x30b8,0x33},
	{0x320c,0x04},//hts = 0x4e2 = 1250
	{0x320d,0xe2},//
	{0x320e,0x05},//30fps@vts = 0x7bc = 1980
	{0x320f,0x28},//
	{0x3253,0x10},
	{0x325f,0x20},
	{0x3301,0x06},
	{0x3304,0x70},
	{0x3306,0x50},
	{0x3309,0xf8},
	{0x330a,0x00},
	{0x330b,0xd8},
	{0x330d,0x10},
	{0x3314,0x13},
	{0x331e,0x61},
	{0x331f,0xe9},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x335e,0x06},
	{0x335f,0x0a},
	{0x3364,0x5e},
	{0x337c,0x02},
	{0x337d,0x0e},
	{0x3390,0x01},
	{0x3391,0x03},
	{0x3392,0x07},
	{0x3393,0x04},
	{0x3394,0x04},
	{0x3395,0x04},
	{0x3396,0x08},
	{0x3397,0x0b},
	{0x3398,0x1f},
	{0x3399,0x06},
	{0x339a,0x0a},
	{0x339b,0x3a},
	{0x339c,0x78},
	{0x33a2,0x04},
	{0x33ac,0x08},
	{0x33ad,0x1c},
	{0x33ae,0x24},
	{0x33af,0xa8},
	{0x33b1,0x80},
	{0x33b3,0x48},
	{0x33f9,0x60},
	{0x33fb,0x78},
	{0x33fc,0x4b},
	{0x33fd,0x5f},
	{0x349f,0x03},
	{0x34a6,0x4b},
	{0x34a7,0x5f},
	{0x34a8,0x20},
	{0x34a9,0x18},
	{0x34aa,0x00},
	{0x34ab,0xe8},
	{0x34ac,0x01},
	{0x34ad,0x00},
	{0x34f8,0x5f},
	{0x34f9,0x18},
	{0x3630,0xc0},
	{0x3631,0x84},
	{0x3632,0x64},
	{0x3633,0x32},
	{0x363b,0x03},
	{0x363c,0x08},
	{0x3641,0x08},
	{0x3670,0x4e},
	{0x3674,0xf0},
	{0x3675,0xc0},
	{0x3676,0xc0},
	{0x3677,0x86},
	{0x3678,0x86},
	{0x3679,0x8d},
	{0x367c,0x48},
	{0x367d,0x49},
	{0x367e,0x4b},
	{0x367f,0x5f},
	{0x3690,0x22},
	{0x3691,0x22},
	{0x3692,0x33},
	{0x369c,0x4b},
	{0x369d,0x4f},
	{0x36b0,0x87},
	{0x36b1,0x90},
	{0x36b2,0xa1},
	{0x36b3,0xc8},
	{0x36b4,0x49},
	{0x36b5,0x4b},
	{0x36b6,0x4f},
	{0x36ea,0x4b},
	{0x36eb,0x0c},
	{0x36ec,0x0c},
	{0x36ed,0x26},
	{0x370f,0x01},
	{0x3722,0x09},
	{0x3724,0x41},
	{0x3725,0xc1},
	{0x3771,0x09},
	{0x3772,0x09},
	{0x3773,0x05},
	{0x377a,0x48},
	{0x377b,0x5f},
	{0x37fa,0x0b},
	{0x37fb,0x33},
	{0x37fc,0x11},
	{0x37fd,0x38},
	{0x3904,0x04},
	{0x3905,0x8c},
	{0x391d,0x04},
	{0x391f,0x49},
	{0x3921,0x20},
	{0x3926,0x21},
	{0x3933,0x80},
	{0x3934,0x08},
	{0x3935,0x00},
	{0x3936,0x90},
	{0x3937,0x78},
	{0x3938,0x77},
	{0x3939,0x00},
	{0x393a,0x00},
	{0x393b,0x00},
	{0x393c,0x1c},
	{0x39dc,0x02},
	{0x3e00,0x00},
	{0x3e01,0x52},
	{0x3e02,0x00},
	{0x3e09,0x00},
	{0x440d,0x10},
	{0x440e,0x01},
	{0x4509,0x20},
	{0x4816,0x51},
	{0x4819,0x09},
	{0x481b,0x05},
	{0x481d,0x14},
	{0x481f,0x04},
	{0x4821,0x0a},
	{0x4823,0x05},
	{0x4825,0x04},
	{0x4827,0x05},
	{0x4829,0x08},
	{0x5780,0x76},
	{0x5784,0x10},
	{0x5785,0x04},
	{0x5787,0x0a},
	{0x5788,0x0a},
	{0x5789,0x04},
	{0x578a,0x0a},
	{0x578b,0x0a},
	{0x578c,0x04},
	{0x578d,0x40},
	{0x5790,0x08},
	{0x5791,0x04},
	{0x5792,0x04},
	{0x5793,0x08},
	{0x5794,0x04},
	{0x5795,0x04},
	{0x5799,0x46},
	{0x579a,0x77},
	{0x57a1,0x04},
	{0x57a8,0xd2},
	{0x57aa,0x2a},
	{0x57ab,0x7f},
	{0x57ac,0x00},
	{0x57ad,0x00},
	{0x59e2,0x08},
	{0x59e3,0x03},
	{0x59e4,0x00},
	{0x59e5,0x10},
	{0x59e6,0x06},
	{0x59e7,0x00},
	{0x59e8,0x08},
	{0x59e9,0x02},
	{0x59ea,0x00},
	{0x59eb,0x10},
	{0x59ec,0x04},
	{0x59ed,0x00},
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
	{0x36e9,0x51},
	{0x37f9,0x53},
	{0x0100,0x01},
	{SC3336P_REG_DELAY,0x10},
	{SC3336P_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc3336p_init_regs_2304_1296_30fps_mipi[] = {
	//cleaned_0x02_SC3336p_MIPI_24Minput_2Lane_10bit_495Mbps_2304x1296_30fps
	{0x0103,0x01},
	{0x36e9,0x80},
	{0x37f9,0x80},
	{0x301f,0x02},
	{0x30b8,0x33},
	{0x320c,0x04},//hts = 0x4e2 = 1250
	{0x320d,0xe2},//
	{0x320e,0x05},//vts = 0x528 = 1320
	{0x320f,0x28},//
	{0x3253,0x10},
	{0x325f,0x20},
	{0x3301,0x04},
	{0x3306,0x50},
	{0x3309,0xf8},
	{0x330a,0x00},
	{0x330b,0xd8},
	{0x3314,0x13},
	{0x331f,0xe9},
	{0x3333,0x10},
	{0x3334,0x40},
	{0x335e,0x06},
	{0x335f,0x0a},
	{0x3364,0x5e},
	{0x337c,0x02},
	{0x337d,0x0e},
	{0x3390,0x01},
	{0x3391,0x03},
	{0x3392,0x07},
	{0x3393,0x04},
	{0x3394,0x04},
	{0x3395,0x04},
	{0x3396,0x08},
	{0x3397,0x0b},
	{0x3398,0x1f},
	{0x3399,0x04},
	{0x339a,0x0a},
	{0x339b,0x3a},
	{0x339c,0x60},
	{0x33a2,0x04},
	{0x33ac,0x08},
	{0x33ad,0x1c},
	{0x33ae,0x10},
	{0x33af,0x30},
	{0x33b1,0x80},
	{0x33b3,0x48},
	{0x33f9,0x60},
	{0x33fb,0x74},
	{0x33fc,0x4b},
	{0x33fd,0x5f},
	{0x349f,0x03},
	{0x34a6,0x4b},
	{0x34a7,0x5f},
	{0x34a8,0x20},
	{0x34a9,0x18},
	{0x34aa,0x00},
	{0x34ab,0xe8},
	{0x34ac,0x01},
	{0x34ad,0x00},
	{0x34f8,0x5f},
	{0x34f9,0x18},
	{0x3630,0xc0},
	{0x3631,0x84},
	{0x3632,0x64},
	{0x3633,0x32},
	{0x363b,0x03},
	{0x363c,0x08},
	{0x3641,0x38},
	{0x3670,0x4e},
	{0x3674,0xf0},
	{0x3675,0xc0},
	{0x3676,0xc0},
	{0x3677,0x86},
	{0x3678,0x86},
	{0x3679,0x86},
	{0x367c,0x48},
	{0x367d,0x49},
	{0x367e,0x4b},
	{0x367f,0x5f},
	{0x3690,0x22},
	{0x3691,0x22},
	{0x3692,0x33},
	{0x369c,0x4b},
	{0x369d,0x4f},
	{0x36b0,0x87},
	{0x36b1,0x90},
	{0x36b2,0xa1},
	{0x36b3,0xc8},
	{0x36b4,0x49},
	{0x36b5,0x4b},
	{0x36b6,0x4f},
	{0x36ea,0x0b},
	{0x36eb,0x0d},
	{0x36ec,0x1c},
	{0x36ed,0x26},
	{0x370f,0x01},
	{0x3722,0x09},
	{0x3724,0x41},
	{0x3725,0xc1},
	{0x3771,0x09},
	{0x3772,0x09},
	{0x3773,0x05},
	{0x377a,0x48},
	{0x377b,0x5f},
	{0x37fa,0x0b},
	{0x37fb,0x33},
	{0x37fc,0x11},
	{0x37fd,0x08},
	{0x3904,0x04},
	{0x3905,0x8c},
	{0x391d,0x04},
	{0x391f,0x49},
	{0x3921,0x20},
	{0x3926,0x21},
	{0x3933,0x80},
	{0x3934,0x08},
	{0x3935,0x00},
	{0x3936,0x90},
	{0x3937,0x79},
	{0x3938,0x77},
	{0x3939,0x00},
	{0x393a,0x00},
	{0x393b,0x00},
	{0x393c,0x28},
	{0x39dc,0x02},
	{0x3e01,0x52},
	{0x3e02,0x00},
	{0x3e09,0x00},
	{0x440d,0x10},
	{0x440e,0x01},
	{0x4509,0x20},
	{0x5780,0x76},
	{0x5784,0x10},
	{0x5785,0x04},
	{0x5787,0x0a},
	{0x5788,0x0a},
	{0x5789,0x04},
	{0x578a,0x0a},
	{0x578b,0x0a},
	{0x578c,0x04},
	{0x578d,0x40},
	{0x5790,0x08},
	{0x5791,0x04},
	{0x5792,0x04},
	{0x5793,0x08},
	{0x5794,0x04},
	{0x5795,0x04},
	{0x5799,0x46},
	{0x579a,0x77},
	{0x57a1,0x04},
	{0x57a8,0xd2},
	{0x57aa,0x2a},
	{0x57ab,0x7f},
	{0x57ac,0x00},
	{0x57ad,0x00},
	{0x59e2,0x08},
	{0x59e3,0x03},
	{0x59e4,0x00},
	{0x59e5,0x10},
	{0x59e6,0x06},
	{0x59e7,0x00},
	{0x59e8,0x08},
	{0x59e9,0x02},
	{0x59ea,0x00},
	{0x59eb,0x10},
	{0x59ec,0x04},
	{0x59ed,0x00},
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
	{0x36e9,0x53},
	{0x37f9,0x27},
	{0x0100,0x01},
	{SC3336P_REG_DELAY,0x10},
	{SC3336P_REG_END, 0x00},	/* END MARKER */
};
static struct tx_isp_sensor_win_setting sc3336p_win_sizes[] = {
	{
		.width		= 2304,
		.height		= 1296,
		.fps		= 45 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc3336p_init_regs_2304_1296_45fps_mipi,
	},
	{
		.width		= 2304,
		.height		= 1296,
		.fps		= 30 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= sc3336p_init_regs_2304_1296_30fps_mipi,
	},

};
struct tx_isp_sensor_win_setting *wsize = &sc3336p_win_sizes[0];

static struct regval_list sc3336p_stream_on[] = {
	{0x0100, 0x01},
	{SC3336P_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list sc3336p_stream_off[] = {
	{0x0100, 0x00},
	{SC3336P_REG_END, 0x00},	/* END MARKER */
};

int sc3336p_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
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

int sc3336p_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
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
static int sc3336p_read_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != SC3336P_REG_END) {
		if (vals->reg_num == SC3336P_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc3336p_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}
#endif

static int sc3336p_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != SC3336P_REG_END) {
		if (vals->reg_num == SC3336P_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = sc3336p_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int sc3336p_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int sc3336p_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = sc3336p_read(sd, 0x3107, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC3336P_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = sc3336p_read(sd, 0x3108, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != SC3336P_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int sc3336p_set_expo(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	int it = (value & 0xffff);
	int again = (value & 0xffff0000) >> 16;

	ret = sc3336p_write(sd, 0x3e00, (unsigned char)((it >> 12) & 0x0f));
	ret += sc3336p_write(sd, 0x3e01, (unsigned char)((it >> 4) & 0xff));
	ret += sc3336p_write(sd, 0x3e02, (unsigned char)((it & 0x0f) << 4));
	ret += sc3336p_write(sd, 0x3e07, (unsigned char)(again & 0xff));
	ret += sc3336p_write(sd, 0x3e09, (unsigned char)((again >> 8) & 0xff));

	return 0;
}

#if 0
static int sc3336p_set_integration_time(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;

	ret = sc3336p_write(sd, 0x3e00, (unsigned char)((value >> 12) & 0x0f));
	ret += sc3336p_write(sd, 0x3e01, (unsigned char)((value >> 4) & 0xff));
	ret += sc3336p_write(sd, 0x3e02, (unsigned char)((value & 0x0f) << 4));
	if (ret < 0)
		return ret;

	return 0;
}

static int sc3336p_set_analog_gain(struct tx_isp_subdev *sd, int value)
{
	int ret = 0;
	unsigned int again = value;

	ret = sc3336p_write(sd, 0x3e07, (unsigned char)(value & 0xff));
	ret += sc3336p_write(sd, 0x3e09, (unsigned char)((value >> 8 & 0xff)));

	return 0;
}
#endif

static int sc3336p_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc3336p_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int sc3336p_init(struct tx_isp_subdev *sd, int enable)
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
	ret = sc3336p_write_array(sd, wsize->regs);
	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int sc3336p_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc3336p_write_array(sd, sc3336p_stream_on);
		} else {
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("sc3336p stream on\n");

	}
	else {
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc3336p_write_array(sd, sc3336p_stream_off);
		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		ISP_WARNING("sc3336p stream off\n");
	}

	return ret;
}

static int sc3336p_set_fps(struct tx_isp_subdev *sd, int fps)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	unsigned int sclk = 0;
	unsigned int hts = 0;
	unsigned int vts = 0;
	unsigned char tmp = 0;
	unsigned int max_fps = 0;
	unsigned int newformat = 0; //the format is 24.8
	int ret = 0;
	switch(default_boot){
	case 0:
		sclk = 1250 * 1320 * 45;
		max_fps = 45;
		break;
	case 1:
		sclk = 1250 * 1320 * 30;
		max_fps = 30;
		break;
	default:
		ISP_ERROR("do not support max framerate %d in mipi mode\n",default_boot);
	}
	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(%d) no in range\n", fps);
		return -1;
	}
	ret = sc3336p_read(sd, 0x320c, &tmp);
	hts = tmp;
	ret += sc3336p_read(sd, 0x320d, &tmp);
	if (0 != ret) {
		ISP_ERROR("err: sc3336p read err\n");
		return ret;
	}
	hts = ((hts << 8) + tmp);
	vts = sclk * (fps & 0xffff) / hts / ((fps & 0xffff0000) >> 16);
	ret += sc3336p_write(sd, 0x320f, (unsigned char)(vts & 0xff));
	ret += sc3336p_write(sd, 0x320e, (unsigned char)(vts >> 8));
	if (0 != ret) {
		ISP_ERROR("err: sc3336p_write err\n");
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

static int sc3336p_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;
	uint8_t val;
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);

	/* 2'b01:mirror,2'b10:filp */
	val = sc3336p_read(sd, 0x3221, &val);
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
	sc3336p_write(sd, 0x3221, val);
	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	return ret;
}

static int sc3336p_set_mode(struct tx_isp_subdev *sd, int value)
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

static int sc3336p_g_chip_ident(struct tx_isp_subdev *sd, struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"sc3336p_reset");
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
		ret = private_gpio_request(pwdn_gpio,"sc3336p_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(10);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = sc3336p_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an sc3336p chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("sc3336p chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "sc3336p", sizeof("sc3336p"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}

	return 0;
}

static int sc3336p_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;
	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = sc3336p_set_expo(sd, *(int*)arg);
		break;
		/*
		   case TX_ISP_EVENT_SENSOR_INT_TIME:
		   if(arg)
		   ret = sc3336p_set_integration_time(sd, *(int*)arg);
		   break;
		   case TX_ISP_EVENT_SENSOR_AGAIN:
		   if(arg)
		   ret = sc3336p_set_analog_gain(sd, *(int*)arg);
		   break;
		   */
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = sc3336p_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = sc3336p_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = sc3336p_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc3336p_write_array(sd, sc3336p_stream_off);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
		}
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		if (data_interface == TX_SENSOR_DATA_INTERFACE_MIPI){
			ret = sc3336p_write_array(sd, sc3336p_stream_on);

		}else{
			ISP_ERROR("Don't support this Sensor Data interface\n");
			ret = -1;
		}
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = sc3336p_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = sc3336p_set_vflip(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int sc3336p_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
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
	ret = sc3336p_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int sc3336p_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;

	sc3336p_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops sc3336p_core_ops = {
	.g_chip_ident = sc3336p_g_chip_ident,
	.reset = sc3336p_reset,
	.init = sc3336p_init,
	/*.ioctl = sc3336p_ops_ioctl,*/
	.g_register = sc3336p_g_register,
	.s_register = sc3336p_s_register,
};

static struct tx_isp_subdev_video_ops sc3336p_video_ops = {
	.s_stream = sc3336p_s_stream,
};

static struct tx_isp_subdev_sensor_ops	sc3336p_sensor_ops = {
	.ioctl	= sc3336p_sensor_ops_ioctl,
};

static struct tx_isp_subdev_ops sc3336p_ops = {
	.core = &sc3336p_core_ops,
	.video = &sc3336p_video_ops,
	.sensor = &sc3336p_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "sc3336p",
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


static int sc3336p_probe(struct i2c_client *client, const struct i2c_device_id *id)
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
	sensor_mclk_config(sensor, 24000000);
	switch(default_boot){
	case 0:
		wsize = &sc3336p_win_sizes[0];
		sc3336p_attr.max_integration_time_native = 1320 - 8;
		sc3336p_attr.integration_time_limit = 1320 - 8;
		sc3336p_attr.total_width = 1250;
		sc3336p_attr.total_height = 1320;
		sc3336p_attr.max_integration_time = 1320 - 8;
		ISP_WARNING("[%s,%d] default_boot=%d\n", __func__, __LINE__, default_boot);
		break;
	case 1:
		wsize = &sc3336p_win_sizes[1];
		sc3336p_attr.max_integration_time_native = 1320 - 8;
		sc3336p_attr.integration_time_limit = 1320 - 8;
		sc3336p_attr.total_width = 1250;
		sc3336p_attr.total_height = 1320;
		sc3336p_attr.max_integration_time = 1320 - 8;
		sc3336p_attr.mipi.clk = 495;
		ISP_WARNING("[%s,%d] default_boot=%d\n", __func__, __LINE__, default_boot);
		break;
	default:
		ISP_ERROR("do not support max framerate %d in mipi mode\n",default_boot);
	}

	sc3336p_attr.max_again = 365791;
	sc3336p_attr.max_dgain = 0; //sc3336p_attr.max_dgain;
	sd = &sensor->sd;
	video = &sensor->video;
	sensor->video.shvflip = shvflip;
	sc3336p_attr.expo_fs = 1;
	sensor->video.attr = &sc3336p_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &sc3336p_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	pr_debug("probe ok ------->sc3336p\n");

	return 0;

err_get_mclk:
	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	kfree(sensor);

	return -1;
}

static int sc3336p_remove(struct i2c_client *client)
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

static const struct i2c_device_id sc3336p_id[] = {
	{ "sc3336p", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sc3336p_id);

static struct i2c_driver sc3336p_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sc3336p",
	},
	.probe		= sc3336p_probe,
	.remove		= sc3336p_remove,
	.id_table	= sc3336p_id,
};

static __init int init_sc3336p(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init sc3336p dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&sc3336p_driver);
}

static __exit void exit_sc3336p(void)
{
	private_i2c_del_driver(&sc3336p_driver);
}

module_init(init_sc3336p);
module_exit(exit_sc3336p);

MODULE_DESCRIPTION("A low-level driver for SmartSens sc3336p sensors");
MODULE_LICENSE("GPL");
